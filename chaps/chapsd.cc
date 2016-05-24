// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps daemon. It handles calls from multiple processes via D-Bus.
//

#include <signal.h>

#include <memory>
#include <string>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/synchronization/lock.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/platform_thread.h>
#include <base/threading/thread.h>
#include <brillo/syslog_logging.h>
#include <dbus-c++/dbus.h>

#include "chaps/chaps_adaptor.h"
#include "chaps/chaps_factory_impl.h"
#include "chaps/chaps_service.h"
#include "chaps/chaps_service_redirect.h"
#include "chaps/chaps_utility.h"
#include "chaps/platform_globals.h"
#include "chaps/slot_manager_impl.h"

#if USE_TPM2
#include "chaps/tpm2_utility_impl.h"
#else
#include "chaps/tpm_utility_impl.h"
#endif

using base::AutoLock;
using base::Lock;
using base::PlatformThread;
using base::PlatformThreadHandle;
using base::WaitableEvent;
using std::string;

namespace {

#if USE_TPM2
const char kTpmThreadName[] = "tpm_background_thread";
#endif

}  // namespace

namespace chaps {

class AsyncInitThread : public PlatformThread::Delegate {
 public:
  AsyncInitThread(Lock* lock,
                  TPMUtility* tpm,
                  SlotManagerImpl* slot_manager,
                  ChapsServiceImpl* service)
      : started_event_(true, false),
        lock_(lock),
        tpm_(tpm),
        slot_manager_(slot_manager),
        service_(service) {}
  void ThreadMain() {
    // It's important that we acquire 'lock' before signaling 'started_event'.
    // This will prevent any D-Bus requests from being processed until we've
    // finished initialization.
    AutoLock lock(*lock_);
    started_event_.Signal();
    LOG(INFO) << "Starting asynchronous initialization.";
    if (!tpm_->Init())
      // Just warn and continue in this case.  The effect will be a functional
      // daemon which handles dbus requests but any attempt to load a token will
      // fail.  To a PKCS #11 client this will look like a library with a few
      // empty slots.
      LOG(WARNING) << "TPM initialization failed (this is expected if no TPM is"
                   << " available).  PKCS #11 tokens will not be available.";
    if (!slot_manager_->Init())
      LOG(FATAL) << "Slot initialization failed.";
    if (!service_->Init())
      LOG(FATAL) << "Service initialization failed.";
  }
  void WaitUntilStarted() {
    started_event_.Wait();
  }

 private:
  WaitableEvent started_event_;
  Lock* lock_;
  TPMUtility* tpm_;
  SlotManagerImpl* slot_manager_;
  ChapsServiceImpl* service_;
};

std::unique_ptr<DBus::BusDispatcher> g_dispatcher;

void RunDispatcher(Lock* lock,
                   chaps::ChapsInterface* service,
                   chaps::TokenManagerInterface* token_manager) {
  CHECK(service) << "Failed to initialize service.";
  try {
    ChapsAdaptor adaptor(lock, service, token_manager);
    g_dispatcher->enter();
  } catch (DBus::Error err) {
    LOG(FATAL) << "DBus::Error - " << err.what();
  }
}

}  // namespace chaps

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  chaps::ScopedOpenSSL openssl;
  chaps::g_dispatcher.reset(new DBus::BusDispatcher());
  CHECK(chaps::g_dispatcher.get());
  DBus::default_dispatcher = chaps::g_dispatcher.get();
  Lock lock;
  if (!cl->HasSwitch("lib")) {
    // We're using chaps (i.e. not passing through to another PKCS #11 library).
    LOG(INFO) << "Starting PKCS #11 services.";
    // Run as 'chaps'.
    chaps::SetProcessUserAndGroup(chaps::kChapsdProcessUser,
                                  chaps::kChapsdProcessGroup,
                                  true);
    // Determine SRK authorization data from the command line.
    string srk_auth_data;
    if (cl->HasSwitch("srk_password")) {
      srk_auth_data = cl->GetSwitchValueASCII("srk_password");
    } else if (cl->HasSwitch("srk_zeros")) {
      int zero_count = 0;
      if (base::StringToInt(cl->GetSwitchValueASCII("srk_zeros"),
                            &zero_count)) {
        srk_auth_data = string(zero_count, 0);
      } else {
        LOG(WARNING) << "Invalid value for srk_zeros: using empty string.";
      }
    }
#if USE_TPM2
    base::AtExitManager at_exit_manager;
    base::Thread tpm_background_thread(kTpmThreadName);
    CHECK(tpm_background_thread.StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO,
                              0 /* use default stack size */)));
    chaps::TPM2UtilityImpl tpm(tpm_background_thread.task_runner());
#else
    // Instantiate a TPM1.2 Utility.
    chaps::TPMUtilityImpl tpm(srk_auth_data);
#endif
    chaps::ChapsFactoryImpl factory;
    chaps::SlotManagerImpl slot_manager(
        &factory, &tpm, cl->HasSwitch("auto_load_system_token"));
    chaps::ChapsServiceImpl service(&slot_manager);
    chaps::AsyncInitThread init_thread(&lock, &tpm, &slot_manager, &service);
    PlatformThreadHandle init_thread_handle;
    if (!PlatformThread::Create(0, &init_thread, &init_thread_handle))
      LOG(FATAL) << "Failed to create initialization thread.";
    // We don't want to start the dispatcher until the initialization thread has
    // had a chance to acquire the lock.
    init_thread.WaitUntilStarted();
    LOG(INFO) << "Starting D-Bus dispatcher.";
    RunDispatcher(&lock, &service, &slot_manager);
    PlatformThread::Join(init_thread_handle);
#if USE_TPM2
    tpm_background_thread.Stop();
#endif
  } else {
    // We're passing through to another PKCS #11 library.
    string lib = cl->GetSwitchValueASCII("lib");
    LOG(INFO) << "Starting PKCS #11 services with " << lib << ".";
    chaps::ChapsServiceRedirect service(lib.c_str());
    if (!service.Init())
      LOG(FATAL) << "Failed to initialize PKCS #11 library: " << lib;
    RunDispatcher(&lock, &service, NULL);
  }

  return 0;
}

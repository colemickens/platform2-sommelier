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
#include <brillo/daemons/dbus_daemon.h>
#include <brillo/syslog_logging.h>

#include "chaps/chaps_adaptor.h"
#include "chaps/chaps_factory_impl.h"
#include "chaps/chaps_service.h"
#include "chaps/chaps_utility.h"
#include "chaps/dbus_bindings/constants.h"
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

void MaskSignals() {
  sigset_t signal_mask;
  CHECK_EQ(0, sigemptyset(&signal_mask));
  for (int signal : {SIGTERM, SIGINT, SIGHUP}) {
    CHECK_EQ(0, sigaddset(&signal_mask, signal));
  }
  CHECK_EQ(0, sigprocmask(SIG_BLOCK, &signal_mask, nullptr));
}

}  // namespace

namespace chaps {

class AsyncInitThread : public PlatformThread::Delegate {
 public:
  AsyncInitThread(Lock* lock,
                  TPMUtility* tpm,
                  SlotManagerImpl* slot_manager)
      : started_event_(true, false),
        lock_(lock),
        tpm_(tpm),
        slot_manager_(slot_manager) {}
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
  }
  void WaitUntilStarted() {
    started_event_.Wait();
  }

 private:
  WaitableEvent started_event_;
  Lock* lock_;
  TPMUtility* tpm_;
  SlotManagerImpl* slot_manager_;
};

class Daemon : public brillo::DBusServiceDaemon {
 public:
  Daemon(Lock* lock,
         ChapsInterface* service,
         TokenManagerInterface* token_manager)
      : DBusServiceDaemon(kChapsServiceName,
                          dbus::ObjectPath(kObjectManagerPath)),
        lock_(lock),
        service_(service),
        token_manager_(token_manager) {}

 protected:
  void RegisterDBusObjectsAsync(
      brillo::dbus_utils::AsyncEventSequencer* sequencer) override {
    adaptor_.reset(new ChapsAdaptor(object_manager_.get(),
                                    lock_,
                                    service_,
                                    token_manager_));
    adaptor_->RegisterAsync(
        sequencer->GetHandler("RegisterAsync() failed", true));
  }

 private:
  std::unique_ptr<ChapsAdaptor> adaptor_;
  Lock* lock_;  // weak, owned by main function
  ChapsInterface* service_;  // weak, owned by main function
  TokenManagerInterface* token_manager_;  // weak, owned by main function

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace chaps

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogToStderr);
  chaps::ScopedOpenSSL openssl;
  Lock lock;

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
  // Mask signals handled by the daemon thread. This makes sure we
  // won't handle shutdown signals on one of the other threads spawned
  // below.
  MaskSignals();
#if USE_TPM2
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
  chaps::AsyncInitThread init_thread(&lock, &tpm, &slot_manager);
  PlatformThreadHandle init_thread_handle;
  if (!PlatformThread::Create(0, &init_thread, &init_thread_handle))
    LOG(FATAL) << "Failed to create initialization thread.";
  // We don't want to start the dispatcher until the initialization thread has
  // had a chance to acquire the lock.
  init_thread.WaitUntilStarted();
  LOG(INFO) << "Starting D-Bus dispatcher.";
  chaps::Daemon(&lock, &service, &slot_manager).Run();
  PlatformThread::Join(init_thread_handle);
#if USE_TPM2
  tpm_background_thread.Stop();
#endif
  return 0;
}

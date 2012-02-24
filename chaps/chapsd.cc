// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps daemon. It handles calls from multiple processes via D-Bus.
//

#include <signal.h>

#include <string>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/string_number_conversions.h>
#include <base/synchronization/lock.h>
#include <base/synchronization/waitable_event.h>
#include <base/threading/platform_thread.h>
#include <chromeos/syslog_logging.h>
#include <dbus-c++/dbus.h>

#include "chaps/chaps_adaptor.h"
#include "chaps/chaps_factory_impl.h"
#include "chaps/chaps_service.h"
#include "chaps/chaps_service_redirect.h"
#include "chaps/chaps_utility.h"
#include "chaps/slot_manager_impl.h"
#include "chaps/tpm_utility_impl.h"

using base::AutoLock;
using base::Lock;
using base::PlatformThread;
using base::PlatformThreadHandle;
using base::WaitableEvent;
using std::string;

namespace chaps {

class AsyncInitThread : public PlatformThread::Delegate {
 public:
  AsyncInitThread(Lock* lock,
                  const string& srk_auth_data,
                  TPMUtilityImpl* tpm,
                  SlotManagerImpl* slot_manager,
                  ChapsServiceImpl* service)
      : started_event_(true, false),
        lock_(lock),
        srk_auth_data_(srk_auth_data),
        tpm_(tpm),
        slot_manager_(slot_manager),
        service_(service) {}
  void ThreadMain() {
    // It's important that we acquire 'lock' before signaling 'started_event'.
    // This will prevent an D-Bus requests from being processed until we've
    // finished initialization.
    AutoLock lock(*lock_);
    started_event_.Signal();
    LOG(INFO) << "Starting asynchronous initialization.";
    if (!tpm_->Init(srk_auth_data_))
      LOG(FATAL) << "TPM initialization failed.";
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
  string srk_auth_data_;
  TPMUtilityImpl* tpm_;
  SlotManagerImpl* slot_manager_;
  ChapsServiceImpl* service_;
};

scoped_ptr<DBus::BusDispatcher> g_dispatcher;

void RunDispatcher(Lock* lock,
                   chaps::ChapsInterface* service,
                   chaps::LoginEventListener* login_listener) {
  CHECK(service) << "Failed to initialize service.";
  try {
    ChapsAdaptor adaptor(lock, service, login_listener);
    g_dispatcher->enter();
  } catch (DBus::Error err) {
    LOG(FATAL) << "DBus::Error - " << err.what();
  }
}

}  // namespace chaps

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);
  chaps::ScopedOpenSSL openssl;
  chaps::g_dispatcher.reset(new DBus::BusDispatcher());
  CHECK(chaps::g_dispatcher.get());
  DBus::default_dispatcher = chaps::g_dispatcher.get();
  Lock lock;
  if (!cl->HasSwitch("lib")) {
    // We're using chaps (i.e. not passing through to another PKCS #11 library).
    LOG(INFO) << "Starting PKCS #11 services.";
    chaps::ChapsFactoryImpl factory;
    chaps::TPMUtilityImpl tpm;
    chaps::SlotManagerImpl slot_manager(&factory, &tpm);
    chaps::ChapsServiceImpl service(&slot_manager);
    string srk_auth_data;
    if (cl->HasSwitch("srk_password"))
      srk_auth_data = cl->GetSwitchValueASCII("srk_password");
    else if (cl->HasSwitch("srk_zeros")) {
      int zero_count = 0;
      if (base::StringToInt(cl->GetSwitchValueASCII("srk_zeros"),
                            &zero_count)) {
        srk_auth_data = string(zero_count, 0);
      } else {
        LOG(WARNING) << "Invalid value for srk_zeros: using empty string.";
      }
    }
    chaps::AsyncInitThread init_thread(&lock,
                                       srk_auth_data,
                                       &tpm,
                                       &slot_manager,
                                       &service);
    PlatformThreadHandle init_thread_handle;
    if (!PlatformThread::Create(0, &init_thread, &init_thread_handle))
      LOG(FATAL) << "Failed to create initialization thread.";
    // We don't want to start the dispatcher until the initialization thread has
    // had a chance to acquire the lock.
    init_thread.WaitUntilStarted();
    LOG(INFO) << "Starting D-Bus dispatcher.";
    RunDispatcher(&lock, &service, &slot_manager);
    PlatformThread::Join(init_thread_handle);
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

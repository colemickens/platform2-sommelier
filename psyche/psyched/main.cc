// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <base/logging.h>
#include <chromeos/daemons/daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/process.h>
#include <chromeos/syslog_logging.h>
#include <protobinder/binder_watcher.h>
#include <protobinder/iservice_manager.h>

#include "psyche/common/constants.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/psyched/registrar.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

using psyche::Registrar;

namespace psyche {
namespace {

// Upstart signal emitted to notify other processes that psyched is accepting
// connections.
const char kReadySignal[] = "psyche-ready";

class Daemon : public chromeos::Daemon {
 public:
  Daemon() = default;
  ~Daemon() override = default;

 private:
  // chromeos::Daemon:
  int OnInit() override {
    int result = chromeos::Daemon::OnInit();
    if (result != 0) {
      LOG(ERROR) << "Error initializing Daemon";
      return result;
    }

    binder_watcher_.reset(new protobinder::BinderWatcher);

    registrar_.Init();
    result = protobinder::GetServiceManager()->AddService(
        psyche::kPsychedServiceManagerName, &registrar_);
    if (result != 0) {
      LOG(ERROR) << "Unable to register with service manager; RPC returned "
                 << result;
      return EX_UNAVAILABLE;
    }

    LOG(INFO) << "Ready for connections; emitting " << kReadySignal;
    chromeos::ProcessImpl initctl_process;
    initctl_process.AddArg("/sbin/initctl");
    initctl_process.AddArg("emit");
    initctl_process.AddArg(kReadySignal);
    if (!initctl_process.Start()) {
      LOG(ERROR) << "Failed to run initctl";
      return EX_UNAVAILABLE;
    }
    result = initctl_process.Wait();
    if (result != 0) {
      LOG(ERROR) << "initctl exited with " << result;
      return EX_UNAVAILABLE;
    }

    return EX_OK;
  }

 private:
  std::unique_ptr<protobinder::BinderWatcher> binder_watcher_;
  Registrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace
}  // namespace psyche

int main(int argc, char* argv[]) {
  chromeos::FlagHelper::Init(argc, argv, "psyche, the Brillo service manager.");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);
  return psyche::Daemon().Run();
}

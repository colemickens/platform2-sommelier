// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <chromeos/flag_helper.h>
#include <chromeos/syslog_logging.h>
#include <protobinder/binder_daemon.h>
#include <protobinder/iservice_manager.h>

#include "psyche/common/constants.h"
#include "psyche/proto_bindings/psyche.pb.h"
#include "psyche/psyched/registrar.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

using psyche::Registrar;

int main(int argc, char* argv[]) {
  chromeos::FlagHelper::Init(argc, argv, "psyche, the Brillo service manager.");
  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogHeader);

  Registrar registrar;
  registrar.Init();
  int ret = protobinder::GetServiceManager()->AddService(
      psyche::kPsychedServiceManagerName, &registrar);
  VLOG(1) << "GetServiceManager()->AddService() returned " << ret;
  return protobinder::BinderDaemon().Run();
}

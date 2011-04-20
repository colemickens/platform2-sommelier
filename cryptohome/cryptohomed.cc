// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service.h"

#include <stdio.h>
#include <unistd.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <chromeos/syslog_logging.h>

// TODO(wad) This is a placeholder DBus service which allows
//           chrome-login (and anything else running as chronos)
//           to request to mount, unmount, or check if a mapper
//           device is mounted. This is very temporary but should
//           serve as a baseline for moving all the shell scripts
//           into C++.
//           We will need a "CheckKey" interface as well to simplify
//           offline authentication checks.


namespace switches {
// Keeps std* open for debugging
static const char *kNoCloseOnDaemonize = "noclose";
// Enable PKCS#11 initialization via cryptohomed
// TODO(gauravsh): crosbug.com/14277 Remove this flag once this
// feature is stabilized.
static const char *kEnablePkcs11Init = "cryptohome-init-pkcs11";
}  // namespace switches

int main(int argc, char **argv) {
  ::g_type_init();
  base::AtExitManager exit_manager;
  CommandLine::Init(argc, argv);

  chromeos::InitLog(chromeos::kLogToSyslog | chromeos::kLogToStderr);

  // Allow the commands to be configurable.
  CommandLine *cl = CommandLine::ForCurrentProcess();
  int noclose = cl->HasSwitch(switches::kNoCloseOnDaemonize);
  PLOG_IF(FATAL, daemon(0, noclose) == -1) << "Failed to daemonize";

  int enable_pkcs11_init = cl->HasSwitch(switches::kEnablePkcs11Init);
  cryptohome::Service service(enable_pkcs11_init);
  if (!service.Initialize()) {
    LOG(FATAL) << "Service initialization failed";
    return 1;
  }

  if (!service.Register(chromeos::dbus::GetSystemBusConnection())) {
    LOG(FATAL) << "DBUS service registration failed";
    return 1;
  }

  if (!service.Run()) {
    LOG(FATAL) << "Service run failed.";
    return 1;
  }

  return 0;
}

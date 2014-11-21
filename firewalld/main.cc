// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <chromeos/syslog_logging.h>

#include "firewalld/firewall_daemon.h"

using firewalld::FirewallDaemon;

int main(int argc, char** argv) {
  chromeos::InitLog(chromeos::kLogToSyslog);

  FirewallDaemon firewalld;
  firewalld.Run();

  return 0;
}

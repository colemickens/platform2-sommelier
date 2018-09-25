// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/shill_daemon.h"
#include "shill/logging.h"

#include <stdlib.h>
#include <sysexits.h>
#include <string>

#include <base/bind.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

namespace {

// Reads a process' name from |comm_file|, a file like "/proc/%u/comm".
std::string GetProcName(const base::FilePath& comm_file) {
  std::string comm_contents;
  if (!base::ReadFileToString(comm_file, &comm_contents))
    return "";
  base::TrimWhitespaceASCII(comm_contents, base::TRIM_ALL, &comm_contents);
  return comm_contents;
}

// TODO(crbug/865442): Remove this method once the tagged bug is root caused and
// fixed.
bool SigtermHandler(const struct signalfd_siginfo& siginfo) {
  LOG(ERROR) << "SIGTERM sender: " << siginfo.ssi_pid << ","
            << GetProcName(base::FilePath(
                   base::StringPrintf("/proc/%u/comm", siginfo.ssi_pid)));
  // Induce service failure in /var/spool/crash/.
  CHECK(false);
  // Unreachable.
  return true;
}

}  // namespace

namespace shill {

ShillDaemon::ShillDaemon(const base::Closure& startup_callback,
                         const shill::DaemonTask::Settings& settings,
                         Config* config)
    : daemon_task_(settings, config), startup_callback_(startup_callback) {}

ShillDaemon::~ShillDaemon() {}

int ShillDaemon::OnInit() {
  // Manager DBus interface will get registered as part of this init call.
  int return_code = brillo::Daemon::OnInit();
  if (return_code != EX_OK) {
    return return_code;
  }

  daemon_task_.Init();

  // Signal that we've acquired all resources.
  startup_callback_.Run();

  // TODO(crbug/865442): Remove this handler once the tagged bug is root caused
  // and fixed.
  RegisterHandler(SIGTERM, base::Bind(&SigtermHandler));

  return EX_OK;
}

void ShillDaemon::OnShutdown(int* return_code) {
  LOG(INFO) << "ShillDaemon received shutdown.";

  if (!daemon_task_.Quit(base::Bind(&DaemonTask::BreakTerminationLoop,
                                    base::Unretained(&daemon_task_)))) {
    // Run a message loop to allow shill to complete its termination
    // procedures. This is different from the secondary loop in
    // brillo::Daemon. This loop will run until we explicitly
    // breakout of the loop, whereas the secondary loop in
    // brillo::Daemon will run until no more tasks are posted on the
    // loop.  This allows asynchronous D-Bus method calls to complete
    // before exiting.
    brillo::MessageLoop::current()->Run();
  }

  brillo::Daemon::OnShutdown(return_code);
}

}  // namespace shill

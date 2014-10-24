// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <sysexits.h>

#include <base/command_line.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/syslog_logging.h>

namespace {

const char kHelpFlag[] = "help";
const char kLogToStdErrFlag[] = "log_to_stderr";
const char kHelpMessage[] = "\n"
    "This is the Privet protocol handler daemon.\n"
    "Usage: privetd [--v=<logging level>]\n"
    "               [--vmodule=<see base/logging.h>]\n"
    "               [--log_to_stderr]";

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // anonymous namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(kHelpFlag)) {
    LOG(INFO) << kHelpMessage;
    return EX_USAGE;
  }
  chromeos::InitFlags flags = chromeos::kLogToSyslog;
  if (cl->HasSwitch(kLogToStdErrFlag)) {
    flags = chromeos::kLogToStderr;
  }
  chromeos::InitLog(flags | chromeos::kLogHeader);
  Daemon daemon;
  return daemon.Run();
}

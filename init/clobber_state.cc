// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <unistd.h>

#include <string>
#include <vector>

#include <base/logging.h>
#include <base/strings/string_split.h>
#include <brillo/process.h>

// Process the command line arguments.
// static
ClobberState::Arguments ClobberState::ParseArgv(int argc,
                                                char const* const argv[]) {
  Arguments args;
  if (argc <= 1)
    return args;

  // Due to historical usage, the command line parsing is a bit weird.
  // We split the first argument into multiple keywords.
  std::vector<std::string> split_args = base::SplitString(
      argv[1], " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (int i = 2; i < argc; ++i)
    split_args.push_back(argv[i]);

  for (const std::string& arg : split_args) {
    if (arg == "factory") {
      args.factory_wipe = true;
    } else if (arg == "fast") {
      args.fast_wipe = true;
    } else if (arg == "keepimg") {
      args.keepimg = true;
    } else if (arg == "safe") {
      args.safe_wipe = true;
    } else if (arg == "rollback") {
      args.rollback_wipe = true;
    }
  }

  return args;
}

ClobberState::ClobberState(const Arguments& args) : args_(args) {}

int ClobberState::Run() {
  int ret = RunClobberStateShell();
  if (ret)
    LOG(WARNING) << "clobber-state.sh returned with code " << ret;

  // Factory wipe should stop here.
  if (args_.factory_wipe)
    return ret;

  // If everything worked, reboot.
  // This return won't actually be reached unless reboot fails.
  return Reboot();
}

int ClobberState::RunClobberStateShell() {
  brillo::ProcessImpl proc;
  proc.AddArg("/sbin/clobber-state.sh");
  // Arguments are passed via command line as well so that they can be logged
  // in clobber-state.sh.
  if (args_.factory_wipe)
    proc.AddArg("factory");
  if (args_.fast_wipe)
    proc.AddArg("fast");
  if (args_.keepimg)
    proc.AddArg("keepimg");
  if (args_.safe_wipe)
    proc.AddArg("safe");
  if (args_.rollback_wipe)
    proc.AddArg("rollback");

  // The last argument is 1 to indicate that if the variable already exists in
  // the environment, it will be overwritten.
  setenv("FACTORY_WIPE", args_.factory_wipe ? "factory" : "", 1);
  setenv("FAST_WIPE", args_.fast_wipe ? "fast" : "", 1);
  setenv("KEEPIMG", args_.keepimg ? "keepimg" : "", 1);
  setenv("SAFE_WIPE", args_.safe_wipe ? "safe" : "", 1);
  setenv("ROLLBACK_WIPE", args_.rollback_wipe ? "rollback" : "", 1);
  return proc.Run();
}

int ClobberState::Reboot() {
  LOG(INFO) << "clobber-state completed, now rebooting";
  brillo::ProcessImpl proc;
  proc.AddArg("/sbin/shutdown");
  proc.AddArg("-r");
  proc.AddArg("now");
  int ret = proc.Run();
  if (ret == 0) {
    // Wait for reboot to finish (it's an async call).
    sleep(60 * 60 * 24);
  }
  // If we've reached here, reboot (probably) failed.
  LOG(WARNING) << "Requesting reboot failed with failure code " << ret;
  return ret;
}

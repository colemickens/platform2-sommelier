// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <unistd.h>

#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <brillo/process.h>

#include "init/crossystem.h"

namespace {

constexpr char kClobberLogPath[] = "/tmp/clobber-state.log";
constexpr char kClobberStateShellLogPath[] = "/tmp/clobber-state-shell.log";
constexpr char kStatefulPath[] = "/mnt/stateful_partition";
constexpr char kBioWashPath[] = "/usr/bin/bio_wash";

}  // namespace

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

ClobberState::ClobberState(const Arguments& args, CrosSystem* cros_system)
    : args_(args), cros_system_(cros_system), stateful_(kStatefulPath) {}

int ClobberState::Run() {
  DCHECK(cros_system_);

  LOG(INFO) << "Beginning clobber-state run";
  LOG(INFO) << "Factory wipe: " << args_.factory_wipe;
  LOG(INFO) << "Fast wipe: " << args_.fast_wipe;
  LOG(INFO) << "Keepimg: " << args_.keepimg;
  LOG(INFO) << "Safe wipe: " << args_.safe_wipe;
  LOG(INFO) << "Rollback wipe: " << args_.rollback_wipe;

  // Most effective means of destroying user data is run at the start: Throwing
  // away the key to encrypted stateful by requesting the TPM to be cleared at
  // next boot. We shouldn't do this for rollback wipes.
  if (!args_.rollback_wipe) {
    if (!cros_system_->SetInt(CrosSystem::kClearTpmOwnerRequest, 1)) {
      LOG(ERROR) << "Requesting TPM wipe via crossystem failed";
    }
  }

  // In cases where biometric sensors are available, reset the internal entropy
  // used by those sensors for encryption, to render related data/templates etc.
  // undecipherable.
  if (!ClearBiometricSensorEntropy()) {
    LOG(ERROR) << "Clearing biometric sensor internal entropy failed";
  }

  LOG(INFO) << "Starting clobber-state.sh";
  int ret = RunClobberStateShell();
  if (ret)
    LOG(ERROR) << "clobber-state.sh returned with code " << ret;

  // Append logs from clobber-state.sh to clobber-state log.
  std::string clobber_state_shell_logs;
  if (!base::ReadFileToString(base::FilePath(kClobberStateShellLogPath),
                              &clobber_state_shell_logs)) {
    LOG(ERROR) << "Reading clobber-state.sh logs failed";
  }

  // Even if reading clobber-state.sh logs failed, some of its contents may have
  // been read successfully, so we still attempt to append them to our log file.
  if (!base::AppendToFile(base::FilePath(kClobberLogPath),
                          clobber_state_shell_logs.c_str(),
                          clobber_state_shell_logs.size())) {
    LOG(ERROR) << "Appending clobber-state.sh logs to clobber-state log failed";
  }

  // Check if we're in developer mode, and if so, create developer mode marker
  // file so that we don't run clobber-state again after reboot.
  if (!MarkDeveloperMode()) {
    LOG(ERROR) << "Creating developer mode marker file failed.";
  }

  // Schedule flush of filesystem caches to disk.
  sync();

  // Relocate log file back to stateful partition so that it will be preserved
  // after a reboot.
  base::Move(base::FilePath(kClobberLogPath),
             stateful_.Append("unencrypted/clobber-state.log"));

  // Factory wipe should stop here.
  if (args_.factory_wipe)
    return 0;

  // If everything worked, reboot.
  // This return won't actually be reached unless reboot fails.
  return Reboot();
}

bool ClobberState::MarkDeveloperMode() {
  std::string firmware_name;
  int dev_mode_flag;
  if (cros_system_->GetInt(CrosSystem::kDevSwitchBoot, &dev_mode_flag) &&
      dev_mode_flag == 1 &&
      cros_system_->GetString(CrosSystem::kMainFirmwareActive,
                              &firmware_name) &&
      firmware_name != "recovery") {
    return base::WriteFile(stateful_.Append(".developer_mode"), "", 0) == 0;
  }
  return true;
}

void ClobberState::SetStatefulForTest(base::FilePath stateful_path) {
  stateful_ = stateful_path;
}

bool ClobberState::ClearBiometricSensorEntropy() {
  if (base::PathExists(base::FilePath(kBioWashPath))) {
    brillo::ProcessImpl bio_wash;
    bio_wash.AddArg(kBioWashPath);
    return bio_wash.Run() == 0;
  }
  // Return true here so that we don't report spurious failures on platforms
  // without the bio_wash executable.
  return true;
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

  proc.RedirectOutput(kClobberStateShellLogPath);
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
  LOG(ERROR) << "Requesting reboot failed with failure code " << ret;
  return ret;
}

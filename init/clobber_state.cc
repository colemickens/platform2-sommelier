// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/time/time.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/process.h>

#include "init/crossystem.h"

namespace {

constexpr char kStatefulPath[] = "/mnt/stateful_partition";
constexpr char kPowerWashCountPath[] = "unencrypted/preserve/powerwash_count";
constexpr char kClobberLogPath[] = "/tmp/clobber-state.log";
constexpr char kClobberStateShellLogPath[] = "/tmp/clobber-state-shell.log";
constexpr char kBioWashPath[] = "/usr/bin/bio_wash";
constexpr char kPreservedFilesTarPath[] = "/tmp/preserve.tar";

}  // namespace

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

// static
bool ClobberState::IncrementFileCounter(const base::FilePath& path) {
  std::string contents;
  if (!base::ReadFileToString(path, &contents)) {
    return 2 == base::WriteFile(path, "1\n", 2);
  }

  base::TrimWhitespaceASCII(contents, base::TrimPositions::TRIM_ALL, &contents);
  int value;
  if (base::StringToInt(contents, &value)) {
    std::string new_value = std::to_string(value + 1);
    new_value.append("\n");
    return new_value.size() ==
           base::WriteFile(path, new_value.c_str(), new_value.size());
  } else {
    return 2 == base::WriteFile(path, "1\n", 2);
  }
}

// static
int ClobberState::PreserveFiles(
    const base::FilePath& preserved_files_root,
    const std::vector<base::FilePath>& preserved_files,
    const base::FilePath& tar_file_path) {
  // Remove any stale tar files from previous clobber-state runs.
  base::DeleteFile(tar_file_path, /*recursive=*/false);

  // We don't want to create an empty tar file.
  if (preserved_files.size() == 0)
    return 0;

  // We want to preserve permissions and recreate the directory structure
  // for all of the files in |preserved_files|. In order to do so we run tar
  // --no-recursion and specify the names of each of the parent directories.
  // For example for home/.shadow/install_attributes.pb
  // we pass to tar home, home/.shadow, home/.shadow/install_attributes.pb.
  std::vector<std::string> paths_to_tar;
  for (const base::FilePath& path : preserved_files) {
    // All paths should be relative to |preserved_files_root|.
    if (path.IsAbsolute()) {
      LOG(WARNING) << "Non-relative path " << path.value()
                   << " passed to PreserveFiles, ignoring.";
      continue;
    }
    if (!base::PathExists(preserved_files_root.Append(path)))
      continue;
    base::FilePath current = path;
    while (current != base::FilePath(base::FilePath::kCurrentDirectory)) {
      // List of paths is built in an order that is reversed from what we want
      // (parent directories first), but will then be passed to tar in reverse
      // order.
      //
      // e.g. for home/.shadow/install_attributes.pb, |paths_to_tar| will have
      // home/.shadow/install_attributes.pb, then home/.shadow, then home.
      paths_to_tar.push_back(current.value());
      current = current.DirName();
    }
  }

  brillo::ProcessImpl tar;
  tar.AddArg("/bin/tar");
  tar.AddArg("-cf");
  tar.AddArg(tar_file_path.value());
  tar.AddArg("-C");
  tar.AddArg(preserved_files_root.value());
  tar.AddArg("--no-recursion");
  tar.AddArg("--");

  // Add paths in reverse order because we built up the list of paths backwards.
  for (auto it = paths_to_tar.rbegin(); it != paths_to_tar.rend(); ++it) {
    tar.AddArg(*it);
  }
  return tar.Run();
}

ClobberState::ClobberState(const Arguments& args, CrosSystem* cros_system)
    : args_(args), cros_system_(cros_system), stateful_(kStatefulPath) {}

std::vector<base::FilePath> ClobberState::GetPreservedFilesList() {
  std::vector<std::string> stateful_paths;
  // Preserve these files in safe mode. (Please request a privacy review before
  // adding files.)
  //
  // - unencrypted/preserve/update_engine/prefs/rollback-happened: Contains a
  //   boolean value indicating whether a rollback has happened since the last
  //   update check where device policy was available. Needed to avoid forced
  //   updates after rollbacks (device policy is not yet loaded at this time).
  if (args_.safe_wipe) {
    stateful_paths.push_back(kPowerWashCountPath);
    stateful_paths.push_back(
        "unencrypted/preserve/tpm_firmware_update_request");
    stateful_paths.push_back(
        "unencrypted/preserve/update_engine/prefs/rollback-happened");
    stateful_paths.push_back(
        "unencrypted/preserve/update_engine/prefs/rollback-version");

    // Preserve pre-installed demo mode resources for offline Demo Mode.
    std::string demo_mode_resources_dir =
        "unencrypted/cros-components/offline-demo-mode-resources/";
    stateful_paths.push_back(demo_mode_resources_dir + "image.squash");
    stateful_paths.push_back(demo_mode_resources_dir + "imageloader.json");
    stateful_paths.push_back(demo_mode_resources_dir + "imageloader.sig.1");
    stateful_paths.push_back(demo_mode_resources_dir + "imageloader.sig.2");
    stateful_paths.push_back(demo_mode_resources_dir + "manifest.fingerprint");
    stateful_paths.push_back(demo_mode_resources_dir + "manifest.json");
    stateful_paths.push_back(demo_mode_resources_dir + "table");

    // For rollback wipes, we also preserve rollback data. This is an encrypted
    // proto which contains install attributes, device policy and owner.key
    // (used to keep the enrollment), also other device-level configurations
    // e.g. shill configuration to restore network connection after rollback.
    // We also preserve the attestation DB (needed because we don't do TPM clear
    // in this case).
    if (args_.rollback_wipe) {
      stateful_paths.push_back("unencrypted/preserve/attestation.epb");
      stateful_paths.push_back("unencrypted/preserve/rollback_data");
    }
  }

  // Test images in the lab enable certain extra behaviors if the
  // .labmachine flag file is present.  Those behaviors include some
  // important recovery behaviors (cf. the recover_duts upstart job).
  // We need those behaviors to survive across power wash, otherwise,
  // the current boot could wind up as a black hole.
  int debug_build;
  if (cros_system_->GetInt(CrosSystem::kDebugBuild, &debug_build) &&
      debug_build == 1) {
    stateful_paths.push_back(".labmachine");
  }

  std::vector<base::FilePath> preserved_files;
  for (const std::string& path : stateful_paths) {
    preserved_files.push_back(base::FilePath(path));
  }

  if (args_.factory_wipe) {
    base::FileEnumerator enumerator(
        stateful_.Append("unencrypted/import_extensions/extensions"), false,
        base::FileEnumerator::FileType::FILES, "*.crx");
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next()) {
      preserved_files.push_back(
          base::FilePath("unencrypted/import_extensions/extensions")
              .Append(name.BaseName()));
    }
  }

  return preserved_files;
}

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

  if (args_.safe_wipe) {
    IncrementFileCounter(stateful_.Append(kPowerWashCountPath));
  }

  std::vector<base::FilePath> preserved_files = GetPreservedFilesList();
  for (const base::FilePath& fp : preserved_files) {
    LOG(INFO) << "Preserving file: " << fp.value();
  }

  int ret = PreserveFiles(stateful_, preserved_files,
                          base::FilePath(kPreservedFilesTarPath));
  if (ret) {
    LOG(ERROR) << "Preserving files failed with code " << ret;
  }

  LOG(INFO) << "Starting clobber-state.sh";
  ret = RunClobberStateShell();
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

void ClobberState::SetArgsForTest(const ClobberState::Arguments& args) {
  args_ = args;
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
  if (args_.fast_wipe)
    proc.AddArg("fast");
  if (args_.keepimg)
    proc.AddArg("keepimg");
  if (args_.safe_wipe)
    proc.AddArg("safe");

  // The last argument is 1 to indicate that if the variable already exists in
  // the environment, it will be overwritten.
  setenv("FAST_WIPE", args_.fast_wipe ? "fast" : "", 1);
  setenv("KEEPIMG", args_.keepimg ? "keepimg" : "", 1);
  setenv("SAFE_WIPE", args_.safe_wipe ? "safe" : "", 1);

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

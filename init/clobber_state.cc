// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "init/clobber_state.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <termios.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/process.h>
#include <rootdev/rootdev.h>
#include <vboot/cgpt_params.h>
#include <vboot/vboot_host.h>
#include <chromeos/secure_erase_file/secure_erase_file.h>

#include "init/crossystem.h"

namespace {

constexpr char kStatefulPath[] = "/mnt/stateful_partition";
constexpr char kPowerWashCountPath[] = "unencrypted/preserve/powerwash_count";
constexpr char kClobberLogPath[] = "/tmp/clobber-state.log";
constexpr char kClobberStateShellLogPath[] = "/tmp/clobber-state-shell.log";
constexpr char kBioWashPath[] = "/usr/bin/bio_wash";
constexpr char kPreservedFilesTarPath[] = "/tmp/preserve.tar";

constexpr char kUbiRootDisk[] = "/dev/mtd0";
constexpr char kUbiDevicePrefix[] = "/dev/ubi";
constexpr char kUbiDeviceStatefulFormat[] = "/dev/ubi%d_0";

// |strip_partition| attempts to remove the partition number from the result.
base::FilePath GetRootDevice(bool strip_partition) {
  char buf[PATH_MAX];
  int ret = rootdev(buf, PATH_MAX, /*use_slave=*/true, strip_partition);
  if (ret == 0) {
    return base::FilePath(buf);
  } else {
    return base::FilePath();
  }
}

void CgptFindShowFunctionNoOp(struct CgptFindParams*,
                              const char*,
                              int,
                              GptEntry*) {}

int GetPartitionNumber(const base::FilePath& drive_name,
                       const std::string& partition_label) {
  CgptFindParams params;
  memset(&params, 0, sizeof(params));
  params.set_label = 1;
  std::unique_ptr<char> partition_label_copy(strdup(partition_label.c_str()));
  params.label = partition_label_copy.get();
  std::unique_ptr<char> drive_name_copy(strdup(drive_name.value().c_str()));
  params.drive_name = drive_name_copy.get();
  params.show_fn = &CgptFindShowFunctionNoOp;
  CgptFind(&params);
  if (params.hits != 1) {
    LOG(ERROR) << "Could not find partition number for partition "
               << partition_label;
    return -1;
  }
  return params.match_partnum;
}

bool MakeTTYRaw(const base::File& tty) {
  struct termios terminal_properties;
  if (tcgetattr(tty.GetPlatformFile(), &terminal_properties) != 0) {
    LOG(WARNING) << "Getting properties of output TTY failed";
    return false;
  }

  cfmakeraw(&terminal_properties);
  if (tcsetattr(tty.GetPlatformFile(), TCSANOW, &terminal_properties) != 0) {
    LOG(WARNING) << "Setting properties of output TTY failed";
    return false;
  }
  return true;
}

void AppendFileToLog(const base::FilePath& file) {
  std::string file_contents;
  if (!base::ReadFileToString(file, &file_contents)) {
    LOG(ERROR) << "Reading from temporary file failed: " << file.value();
  }

  // Even if reading file failed, some of its contents may have been read
  // successfully, so we still attempt to append them to our log file.
  if (!base::AppendToFile(base::FilePath(kClobberLogPath),
                          file_contents.c_str(), file_contents.size())) {
    LOG(ERROR) << "Appending " << file.value()
               << " to clobber-state log failed";
  }
}

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

// static
bool ClobberState::GetDevicePathComponents(const base::FilePath& device,
                                           std::string* base_device_out,
                                           int* partition_out) {
  if (!partition_out || !base_device_out)
    return false;
  const std::string& path = device.value();

  // MTD devices sometimes have a trailing "_0" after the partition which
  // we should ignore.
  std::string mtd_suffix = "_0";
  size_t suffix_index = path.length();
  if (base::EndsWith(path, mtd_suffix, base::CompareCase::SENSITIVE)) {
    suffix_index = path.length() - mtd_suffix.length();
  }

  size_t last_non_numeric =
      path.find_last_not_of("0123456789", suffix_index - 1);

  // If there are no non-numeric characters, this is a malformed device.
  if (last_non_numeric == std::string::npos) {
    return false;
  }

  std::string partition_number_string =
      path.substr(last_non_numeric + 1, suffix_index - (last_non_numeric + 1));
  int partition_number;
  if (!base::StringToInt(partition_number_string, &partition_number)) {
    return false;
  }
  *partition_out = partition_number;
  *base_device_out = path.substr(0, last_non_numeric + 1);
  return true;
}

bool ClobberState::IsRotational(const base::FilePath& device_path) {
  if (!dev_.IsParent(device_path)) {
    LOG(ERROR) << "Non-device given as argument to IsRotational: "
               << device_path.value();
    return false;
  }

  // Since there doesn't seem to be a good way to get from a partition name
  // to the base device name beyond simple heuristics, just find the device
  // with the same major number but with minor 0.
  struct stat st;
  if (Stat(device_path, &st) != 0) {
    return false;
  }
  unsigned int major_device_number = major(st.st_rdev);

  base::FileEnumerator enumerator(dev_, /*recursive=*/true,
                                  base::FileEnumerator::FileType::FILES);
  for (base::FilePath base_device_path = enumerator.Next();
       !base_device_path.empty(); base_device_path = enumerator.Next()) {
    if (Stat(base_device_path, &st) == 0 && S_ISBLK(st.st_mode) &&
        major(st.st_rdev) == major_device_number && minor(st.st_rdev) == 0) {
      // |base_device_path| must be the base device for |device_path|.
      base::FilePath rotational_file = sys_.Append("block")
                                           .Append(base_device_path.BaseName())
                                           .Append("queue/rotational");
      std::string value;
      if (base::ReadFileToString(rotational_file, &value)) {
        base::TrimWhitespaceASCII(value, base::TrimPositions::TRIM_ALL, &value);
        return value == "1";
      }
    }
  }
  return false;
}

// static
bool ClobberState::GetDevicesToWipe(
    const base::FilePath& root_disk,
    const base::FilePath& root_device,
    const ClobberState::PartitionNumbers& partitions,
    ClobberState::DeviceWipeInfo* wipe_info_out) {
  if (!wipe_info_out) {
    LOG(ERROR) << "wipe_info_out must be non-null";
    return false;
  }

  if (partitions.root_a < 0 || partitions.root_b < 0 ||
      partitions.kernel_a < 0 || partitions.kernel_b < 0 ||
      partitions.stateful < 0) {
    LOG(ERROR) << "Invalid partition numbers for GetDevicesToWipe";
    return false;
  }

  if (root_disk.empty()) {
    LOG(ERROR) << "Invalid root disk for GetDevicesToWipe";
    return false;
  }

  if (root_device.empty()) {
    LOG(ERROR) << "Invalid root device for GetDevicesToWipe";
    return false;
  }

  std::string base_device;
  int partition_number;
  if (!GetDevicePathComponents(root_device, &base_device, &partition_number)) {
    LOG(ERROR) << "Extracting partition number and base device from "
                  "root_device failed: "
               << root_device.value();
    return false;
  }

  if (partition_number != partitions.root_a &&
      partition_number != partitions.root_b) {
    LOG(ERROR) << "Root device partition number (" << partition_number
               << ") does not match either root partition number: "
               << partitions.root_a << ", " << partitions.root_b;
    return false;
  }

  ClobberState::DeviceWipeInfo wipe_info;
  base::FilePath kernel_device;
  if (root_disk == base::FilePath(kUbiRootDisk)) {
    /*
     * WARNING: This code has not been sufficiently tested and almost certainly
     * does not work. If you are adding support for MTD flash, you would be
     * well served to review it and add test coverage.
     */

    // Special casing for NAND devices.
    wipe_info.is_mtd_flash = true;
    wipe_info.stateful_device = base::FilePath(
        base::StringPrintf(kUbiDeviceStatefulFormat, partitions.stateful));

    // On NAND, kernel is stored on /dev/mtdX.
    if (partition_number == partitions.root_a) {
      kernel_device =
          base::FilePath("/dev/mtd" + std::to_string(partitions.kernel_a));
    } else if (partition_number == partitions.root_b) {
      kernel_device =
          base::FilePath("/dev/mtd" + std::to_string(partitions.kernel_b));
    }

    /*
     * End of untested MTD code.
     */
  } else {
    wipe_info.stateful_device =
        base::FilePath(base_device + std::to_string(partitions.stateful));

    if (partition_number == partitions.root_a) {
      kernel_device =
          base::FilePath(base_device + std::to_string(partitions.kernel_a));
    } else if (partition_number == partitions.root_b) {
      kernel_device =
          base::FilePath(base_device + std::to_string(partitions.kernel_b));
    }
  }

  int root_partition_to_wipe = 0;
  if (partition_number == partitions.root_a) {
    wipe_info.inactive_root_device =
        base::FilePath(base_device + std::to_string(partitions.root_b));
    wipe_info.inactive_kernel_device =
        base::FilePath(base_device + std::to_string(partitions.kernel_b));
    wipe_info.active_kernel_partition = partitions.kernel_a;
    root_partition_to_wipe = partitions.root_b;
  } else if (partition_number == partitions.root_b) {
    wipe_info.inactive_root_device =
        base::FilePath(base_device + std::to_string(partitions.root_a));
    wipe_info.inactive_kernel_device =
        base::FilePath(base_device + std::to_string(partitions.kernel_a));
    wipe_info.active_kernel_partition = partitions.kernel_b;
    root_partition_to_wipe = partitions.root_a;
  }

  if (root_partition_to_wipe != partitions.root_a &&
      root_partition_to_wipe != partitions.root_b) {
    LOG(ERROR) << "Partition number to wipe (" << root_partition_to_wipe
               << ") does not match either root partition number: "
               << partitions.root_a << ", " << partitions.root_b;
    return false;
  }

  *wipe_info_out = wipe_info;
  return true;
}

ClobberState::ClobberState(const Arguments& args,
                           std::unique_ptr<CrosSystem> cros_system)
    : args_(args),
      cros_system_(std::move(cros_system)),
      stateful_(kStatefulPath),
      dev_("/dev"),
      sys_("/sys") {
  if (base::PathExists(base::FilePath("/sbin/frecon"))) {
    terminal_path_ = base::FilePath("/run/frecon/vt0");
  } else {
    terminal_path_ = base::FilePath("/dev/tty1");
  }
}

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

  // As we move factory wiping from release image to factory test image,
  // clobber-state will be invoked directly under a tmpfs. GetRootDevice cannot
  // report correct output under such a situation. Therefore, the output is
  // preserved then assigned to environment variables ROOT_DEV/ROOT_DISK for
  // clobber-state. For other cases, the environment variables will be empty and
  // it falls back to using GetRootDevice.
  const char* root_disk_cstr = getenv("ROOT_DISK");
  if (root_disk_cstr != nullptr) {
    root_disk_ = base::FilePath(root_disk_cstr);
  } else {
    root_disk_ = GetRootDevice(/*strip_partition=*/true);
  }

  // Special casing for NAND devices
  if (base::StartsWith(root_disk_.value(), kUbiDevicePrefix,
                       base::CompareCase::SENSITIVE)) {
    root_disk_ = base::FilePath(kUbiRootDisk);
  }

  base::FilePath root_device;
  const char* root_device_cstr = getenv("ROOT_DEV");
  if (root_device_cstr != nullptr) {
    root_device = base::FilePath(root_device_cstr);
  } else {
    root_device = GetRootDevice(/*strip_partition=*/false);
  }

  LOG(INFO) << "Root disk: " << root_disk_.value();
  LOG(INFO) << "Root device: " << root_device.value();

  partitions_.stateful = GetPartitionNumber(root_disk_, "STATE");
  partitions_.root_a = GetPartitionNumber(root_disk_, "ROOT-A");
  partitions_.root_b = GetPartitionNumber(root_disk_, "ROOT-B");
  partitions_.kernel_a = GetPartitionNumber(root_disk_, "KERN-A");
  partitions_.kernel_b = GetPartitionNumber(root_disk_, "KERN-B");

  if (!GetDevicesToWipe(root_disk_, root_device, partitions_, &wipe_info_)) {
    LOG(ERROR) << "Getting devices to wipe failed, aborting run";
    return 1;
  }

  // Determine if stateful partition's device is backed by a rotational disk.
  bool is_rotational = false;
  if (!wipe_info_.is_mtd_flash) {
    is_rotational = IsRotational(wipe_info_.stateful_device);
  }

  LOG(INFO) << "Stateful device: " << wipe_info_.stateful_device.value();
  LOG(INFO) << "Inactive root device: "
            << wipe_info_.inactive_root_device.value();
  LOG(INFO) << "Inactive kernel device: "
            << wipe_info_.inactive_kernel_device.value();

  brillo::ProcessImpl clobber_log;
  clobber_log.AddArg("/sbin/clobber-log");
  clobber_log.AddArg("--preserve");
  clobber_log.AddArg("clobber-state");
  if (args_.factory_wipe)
    clobber_log.AddArg("factory");
  if (args_.fast_wipe)
    clobber_log.AddArg("fast");
  if (args_.keepimg)
    clobber_log.AddArg("keepimg");
  if (args_.safe_wipe)
    clobber_log.AddArg("safe");
  if (args_.rollback_wipe)
    clobber_log.AddArg("rollback");
  clobber_log.Run();

  AttemptSwitchToFastWipe(is_rotational);

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

  LOG(INFO) << "clobber-state has completed";

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

void ClobberState::AttemptSwitchToFastWipe(bool is_rotational) {
  // On a non-fast wipe, rotational drives take too long. Override to run them
  // through "fast" mode, with a forced delay. Sensitive contents should already
  // be encrypted.
  if (!args_.fast_wipe && is_rotational) {
    LOG(INFO) << "Stateful device is on rotational disk, shredding files";
    ShredRotationalStatefulFiles();
    ForceDelay();
    args_.fast_wipe = true;
    LOG(INFO) << "Switching to fast wipe";
  }

  // For drives that support secure erasure, wipe the keysets,
  // and then run the drives through "fast" mode, with a forced delay.
  //
  // Note: currently only eMMC-based SSDs are supported.
  if (!args_.fast_wipe) {
    LOG(INFO) << "Attempting to wipe encryption keysets";
    if (WipeKeysets()) {
      LOG(INFO) << "Wiping encryption keysets succeeded";
      ForceDelay();
      args_.fast_wipe = true;
      LOG(INFO) << "Switching to fast wipe";
    } else {
      LOG(INFO) << "Wiping encryption keysets failed";
    }
  }
}

void ClobberState::ShredRotationalStatefulFiles() {
  // Directly remove things that are already encrypted (which are also the
  // large things), or are static from images.
  base::DeleteFile(stateful_.Append("encrypted.block"), /*recursive=*/false);
  base::DeleteFile(stateful_.Append("var_overlay"), /*recursive=*/true);
  base::DeleteFile(stateful_.Append("dev_image"), /*recursive=*/true);

  base::FileEnumerator shadow_files(
      stateful_.Append("home/.shadow"),
      /*recursive=*/true, base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath path = shadow_files.Next(); !path.empty();
       path = shadow_files.Next()) {
    if (path.BaseName() == base::FilePath("vault")) {
      base::DeleteFile(path, /*recursive=*/true);
    }
  }

  base::FilePath temp_file;
  base::CreateTemporaryFile(&temp_file);

  // Shred everything else. We care about contents not filenames, so do not
  // use "-u" since metadata updates via fdatasync dominate the shred time.
  // Note that if the count-down is interrupted, the reset file continues
  // to exist, which correctly continues to indicate a needed wipe.
  brillo::ProcessImpl shred;
  shred.AddArg("/usr/bin/shred");
  shred.AddArg("--force");
  shred.AddArg("--zero");
  base::FileEnumerator stateful_files(stateful_, /*recursive=*/true,
                                      base::FileEnumerator::FileType::FILES);
  for (base::FilePath path = stateful_files.Next(); !path.empty();
       path = stateful_files.Next()) {
    shred.AddArg(path.value());
  }
  shred.RedirectOutput(temp_file.value());
  shred.Run();
  AppendFileToLog(temp_file);

  sync();
}

bool ClobberState::WipeKeysets() {
  std::vector<std::string> key_files{
      "encrypted.key", "encrypted.needs-finalization",
      "home/.shadow/cryptohome.key", "home/.shadow/salt",
      "home/.shadow/salt.sum"};
  for (const std::string& str : key_files) {
    base::FilePath path = stateful_.Append(str);
    if (base::PathExists(path)) {
      if (!SecureErase(path)) {
        LOG(ERROR) << "Securely erasing file failed: " << path.value();
        return false;
      }
    }
  }

  // Delete files named 'master' in directories contained in '.shadow'.
  base::FileEnumerator directories(stateful_.Append("home/.shadow"),
                                   /*recursive=*/false,
                                   base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath dir = directories.Next(); !dir.empty();
       dir = directories.Next()) {
    base::FileEnumerator files(dir, /*recursive=*/false,
                               base::FileEnumerator::FileType::FILES);
    for (base::FilePath file = files.Next(); !file.empty();
         file = files.Next()) {
      if (file.RemoveExtension().BaseName() == base::FilePath("master")) {
        if (!SecureErase(file)) {
          LOG(ERROR) << "Securely erasing file failed: " << file.value();
          return false;
        }
      }
    }
  }

  return DropCaches();
}

void ClobberState::ForceDelay() {
  base::File terminal(terminal_path_,
                      base::File::FLAG_OPEN | base::File::FLAG_WRITE);
  if (!terminal.IsValid()) {
    LOG(ERROR) << "Opening terminal for delay countdown failed";
  }

  MakeTTYRaw(terminal);
  for (int delay = 300; delay >= 0; delay--) {
    std::string count =
        base::StringPrintf("%2d:%02d\r", delay / 60, delay % 60);
    terminal.WriteAtCurrentPos(count.c_str(), count.size());
    sleep(1);
  }
}

// Wrapper around secure_erase_file::SecureErase(const base::FilePath&).
bool ClobberState::SecureErase(const base::FilePath& path) {
  return secure_erase_file::SecureErase(path);
}

// Wrapper around secure_erase_file::DropCaches(). Must be called after
// a call to SecureEraseFile. Files are only securely deleted if DropCaches
// returns true.
bool ClobberState::DropCaches() {
  return secure_erase_file::DropCaches();
}

void ClobberState::SetArgsForTest(const ClobberState::Arguments& args) {
  args_ = args;
}

ClobberState::Arguments ClobberState::GetArgsForTest() {
  return args_;
}

void ClobberState::SetStatefulForTest(base::FilePath stateful_path) {
  stateful_ = stateful_path;
}

void ClobberState::SetDevForTest(base::FilePath dev_path) {
  dev_ = dev_path;
}

void ClobberState::SetSysForTest(base::FilePath sys_path) {
  sys_ = sys_path;
}

int ClobberState::Stat(const base::FilePath& path, struct stat* st) {
  return stat(path.value().c_str(), st);
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
  // The last argument to setenv is 1 to indicate that if the variable already
  // exists in the environment, it will be overwritten.

  // Command line arguments
  setenv("FAST_WIPE", args_.fast_wipe ? "fast" : "", 1);
  setenv("KEEPIMG", args_.keepimg ? "keepimg" : "", 1);
  setenv("SAFE_WIPE", args_.safe_wipe ? "safe" : "", 1);

  // Information about what devices to wipe and how to wipe them.
  setenv("IS_MTD", wipe_info_.is_mtd_flash ? "1" : "0", 1);

  setenv("ROOT_DISK", root_disk_.value().c_str(), 1);
  setenv("STATE_DEV", wipe_info_.stateful_device.value().c_str(), 1);
  setenv("OTHER_ROOT_DEV", wipe_info_.inactive_root_device.value().c_str(), 1);
  setenv("OTHER_KERNEL_DEV", wipe_info_.inactive_kernel_device.value().c_str(),
         1);

  setenv("PARTITION_NUM_ROOT_A", std::to_string(partitions_.root_a).c_str(), 1);
  setenv("PARTITION_NUM_ROOT_B", std::to_string(partitions_.root_b).c_str(), 1);
  setenv("PARTITION_NUM_STATE", std::to_string(partitions_.stateful).c_str(),
         1);
  setenv("KERNEL_PART_NUM",
         std::to_string(wipe_info_.active_kernel_partition).c_str(), 1);

  setenv("TTY", terminal_path_.value().c_str(), 1);

  brillo::ProcessImpl proc;
  proc.AddArg("/sbin/clobber-state.sh");
  proc.RedirectOutput(kClobberStateShellLogPath);
  return proc.Run();
}

int ClobberState::Reboot() {
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

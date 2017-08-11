// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "verity_mounter.h"

#include <algorithm>
#include <string>

#include </usr/include/linux/magic.h>
#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>

#include <base/command_line.h>
#include <base/containers/adapters.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/process/launch.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>

#include "component.h"

namespace imageloader {

namespace {

enum class MountStatus { FAIL, RETRY, SUCCESS };

constexpr int GET_LOOP_DEVICE_MAX_RETRY = 5;
constexpr int DMSETUP_TIMEOUT_SECONDS = 3;

// |argv| should include all the commands and table to dmsetup, but not
// include the path to the binary.
bool RunDMSetup(const std::vector<std::string>& argv) {
  base::LaunchOptions options;
  options.clear_environ = true;

  std::vector<std::string> full_argv(argv);
  full_argv.insert(full_argv.begin(), "/sbin/dmsetup");

  base::Process process = base::LaunchProcess(full_argv, options);

  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch dmsetup process";
    return false;
  }

  int exit_code;
  if (!process.WaitForExitWithTimeout(
          base::TimeDelta::FromSeconds(DMSETUP_TIMEOUT_SECONDS), &exit_code)) {
    LOG(ERROR) << "Failed to wait for dmsetup process.";
    return false;
  }

  return exit_code == 0;
}

bool LaunchDMCreate(const std::string& name, const std::string& table) {
  const std::vector<std::string> argv = {"create", name, "--table",
                                         table.c_str(), "--readonly"};
  return RunDMSetup(argv);
}

// Clear the /dev/mapper/<foo> verity device.
void ClearVerityDevice(const std::string& name) {
  // Per the man page, wipe_table:
  // Wait for any I/O in-flight through the device to complete, then replace the
  // table with a new table that fails any new I/O sent to the device.  If
  // successful, this should release any devices held open by the device's
  // table(s).
  const std::vector<std::string> wipe_argv = {"wipe_table", name};
  RunDMSetup(wipe_argv);
  // Now remove the actual device.
  const std::vector<std::string> remove_argv = {"remove", name};
  RunDMSetup(remove_argv);
}

// Clear the file descriptor behind a loop device.
void ClearLoopDevice(const std::string& device_path) {
  base::ScopedFD loop_device_fd(
      open(device_path.c_str(), O_RDONLY | O_CLOEXEC));
  if (loop_device_fd.is_valid()) ioctl(loop_device_fd.get(), LOOP_CLR_FD, 0);
}

bool SetupDeviceMapper(const std::string& device_path, const std::string& table,
                       std::string* dev_name) {
  // Now setup the dmsetup table.
  std::string final_table = table;
  if (!VerityMounter::SetupTable(&final_table, device_path)) return false;

  // Generate a name with a random string of 32 characters: we consider this to
  // have sufficiently low chance of collision to assume the name isn't taken.
  std::vector<uint8_t> rand_bytes(32);
  base::RandBytes(rand_bytes.data(), rand_bytes.size());
  std::string name = base::HexEncode(rand_bytes.data(), rand_bytes.size());

  if (!LaunchDMCreate(name, final_table)) {
    LOG(ERROR) << "Failed to run dmsetup.";
    return false;
  }

  dev_name->assign("/dev/mapper/" + name);
  return true;
}

bool CreateDirectoryWithMode(const base::FilePath& full_path, int mode) {
  std::vector<base::FilePath> subpaths;

  // Collect a list of all parent directories.
  base::FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (base::FilePath path = full_path.DirName();
       path.value() != last_path.value(); path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (const auto& subpath : base::Reversed(subpaths)) {
    if (base::DirectoryExists(subpath)) continue;
    if (mkdir(subpath.value().c_str(), mode) == 0) continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    if (!base::DirectoryExists(subpath)) {
      PLOG(ERROR) << "Failed to create directory: " << subpath.value();
      return false;
    }
  }
  return true;
}

bool CreateMountPointIfNeeded(const base::FilePath& mount_point,
                              bool* already_mounted) {
  *already_mounted = false;
  // Is this mount point somehow already taken?
  struct stat st;
  if (lstat(mount_point.value().c_str(), &st) == 0) {
    if (!S_ISDIR(st.st_mode)) {
      LOG(ERROR) << "Mount point exists but is not a directory.";
      return false;
    }

    base::FilePath mount_parent = mount_point.DirName();
    struct stat st2;
    if (stat(mount_parent.value().c_str(), &st2) != 0) {
      PLOG(ERROR) << "Could not stat the mount point parent";
      return false;
    }
    if (st.st_dev != st2.st_dev) {
      struct statfs st_fs;
      if (statfs(mount_point.value().c_str(), &st_fs) != 0) {
        PLOG(ERROR) << "statfs";
        return false;
      }
      if (st_fs.f_type != SQUASHFS_MAGIC || !(st_fs.f_flags & ST_NODEV) ||
          !(st_fs.f_flags & ST_NOSUID) || !(st_fs.f_flags & ST_RDONLY)) {
        LOG(ERROR) << "File system is not the expected type.";
        return false;
      }
      *already_mounted = true;
      return true;
    }
  } else if (!CreateDirectoryWithMode(mount_point, kComponentDirPerms)) {
    LOG(ERROR) << "Failed to create mount point: " << mount_point.value();
    return false;
  }
  return true;
}

// Reserves a loop device and associates it with |image_fd|. The path to the
// loop device is returned in |device_path_out|. When the loop device is no
// longer being used, free the resource with ClearLoopDevice().
MountStatus GetLoopDevice(const base::ScopedFD& image_fd,
                          std::string* device_path_out) {
  base::ScopedFD loopctl_fd(open("/dev/loop-control", O_RDONLY | O_CLOEXEC));
  if (!loopctl_fd.is_valid()) {
    PLOG(ERROR) << "loopctl_fd";
    return MountStatus::FAIL;
  }
  int device_free_number = ioctl(loopctl_fd.get(), LOOP_CTL_GET_FREE);
  if (device_free_number < 0) {
    PLOG(ERROR) << "ioctl : LOOP_CTL_GET_FREE";
    return MountStatus::FAIL;
  }

  std::string device_path =
      base::StringPrintf("/dev/loop%d", device_free_number);
  base::ScopedFD loop_device_fd(
      open(device_path.c_str(), O_RDONLY | O_CLOEXEC));
  if (!loop_device_fd.is_valid()) {
    PLOG(ERROR) << "Failed to open loop device: " << device_path;
    return MountStatus::FAIL;
  }

  if (ioctl(loop_device_fd.get(), LOOP_SET_FD, image_fd.get()) == -1) {
    if (errno != EBUSY) {
      PLOG(ERROR) << "ioctl: LOOP_SET_FD";
      ioctl(loop_device_fd.get(), LOOP_CLR_FD, 0);
      return MountStatus::FAIL;
    }
    return MountStatus::RETRY;
  }

  device_path_out->assign(device_path);
  return MountStatus::SUCCESS;
}

}  // namespace

// static
bool VerityMounter::SetupTable(std::string* table,
                               const std::string& device_path) {
  // Make sure there is only one entry in the device mapper table.
  if (std::count(table->begin(), table->end(), '\n') > 1) return false;

  // Remove all newlines from the table. This is to workaround the server
  // incorrectly inserting a newline when writing out the table.
  table->erase(std::remove(table->begin(), table->end(), '\n'), table->end());
  // Replace in the actual loop device name.
  base::ReplaceSubstringsAfterOffset(table, 0, "ROOT_DEV", device_path);
  base::ReplaceSubstringsAfterOffset(table, 0, "HASH_DEV", device_path);
  // If the table does not specify an error condition, use the default (eio).
  // This is critical because the default behavior is to panic the device and
  // force a system recovery. Do not do this for component corruption.
  if (table->find("error_behavior") == std::string::npos)
    table->append(" error_behavior=eio");

  return true;
}

bool VerityMounter::Mount(const base::ScopedFD& image_fd,
                          const base::FilePath& mount_point,
                          const std::string& table) {
  // First check if the component is already mounted and avoid unnecessary work.
  bool already_mounted = false;
  if (!CreateMountPointIfNeeded(mount_point, &already_mounted)) return false;
  if (already_mounted) return true;

  std::string loop_device_path;
  // We need to retry because another program could grap the loop device,
  // resulting in an EBUSY error. If that happens, run again and grab a new
  // device.
  int retries = GET_LOOP_DEVICE_MAX_RETRY;
  while (true) {
    MountStatus status = GetLoopDevice(image_fd, &loop_device_path);
    if (status == MountStatus::FAIL ||
        (status == MountStatus::RETRY && retries == 0)) {
      LOG(ERROR) << "GetLoopDevice failed, mount_point: "
                 << mount_point.value();
      return false;
    } else if (status == MountStatus::SUCCESS) {
      break;
    }
    --retries;
  }

  std::string dev_name;
  if (!SetupDeviceMapper(loop_device_path, table, &dev_name)) {
    LOG(ERROR) << "mount_point: " << mount_point.value();
    ClearLoopDevice(loop_device_path);
    return false;
  }

  if (mount(dev_name.c_str(), mount_point.value().c_str(), "squashfs",
            MS_RDONLY | MS_NOSUID | MS_NODEV, "") < 0) {
    PLOG(ERROR) << "mount, mount_point " << mount_point.value();
    ClearVerityDevice(dev_name);
    ClearLoopDevice(loop_device_path);
    return false;
  }

  return true;
}

}  // namespace imageloader

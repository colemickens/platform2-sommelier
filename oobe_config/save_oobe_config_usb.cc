// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/save_oobe_config_usb.h"

#include <pwd.h>
#include <stdlib.h>

#include <string>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>

#include "oobe_config/usb_utils.h"

using base::FileEnumerator;
using base::FilePath;
using std::string;
using std::vector;

namespace oobe_config {

namespace {
constexpr char kOobeConfigRestoreUser[] = "oobe_config_restore";
}  // namespace

SaveOobeConfigUsb::SaveOobeConfigUsb(const FilePath& device_stateful,
                                     const FilePath& usb_stateful,
                                     const FilePath& device_ids_dir,
                                     const FilePath& usb_device,
                                     const FilePath& private_key_file,
                                     const FilePath& public_key_file)
    : device_stateful_(device_stateful),
      usb_stateful_(usb_stateful),
      device_ids_dir_(device_ids_dir),
      usb_device_(usb_device),
      private_key_file_(private_key_file),
      public_key_file_(public_key_file) {}

bool SaveOobeConfigUsb::SaveInternal() const {
  // Check that input files and directories exist.
  for (const auto& dir : {device_stateful_, usb_stateful_, device_ids_dir_}) {
    if (!base::DirectoryExists(dir)) {
      LOG(ERROR) << "Directory " << dir.value() << " does not exist!";
      return false;
    }
  }
  for (const auto& file : {usb_device_, private_key_file_, public_key_file_}) {
    if (!base::PathExists(file)) {
      LOG(ERROR) << "File " << file.value() << " does not exist!";
      return false;
    }
  }

  // /stateful/unencrypted/oobe_auto_config might not exist on the target
  // device, so create it here.
  auto device_config_dir = device_stateful_.Append(kUnencryptedOobeConfigDir);
  if (!base::CreateDirectory(device_config_dir)) {
    PLOG(ERROR) << "Failed to create directory: " << device_config_dir.value();
    return false;
  }

  auto usb_config_dir = usb_stateful_.Append(kUnencryptedOobeConfigDir);
  auto config_file = usb_config_dir.Append(kConfigFile);
  if (!Sign(private_key_file_, config_file,
            device_config_dir.Append(kConfigFile).AddExtension("sig"))) {
    return false;
  }

  // If the media was provisioned for auto-enrollment, sign the domain name
  // as well.
  auto enrollment_domain_file = usb_config_dir.Append(kDomainFile);
  if (base::PathExists(enrollment_domain_file)) {
    CHECK(Sign(private_key_file_, enrollment_domain_file,
               device_config_dir.Append(kDomainFile).AddExtension("sig")));
  }

  // Sign the source stateful device name.
  FilePath mount_dev;
  if (!FindPersistentMountDevice(&mount_dev)) {
    return false;
  }
  if (!Sign(private_key_file_, mount_dev.value(),
            device_config_dir.Append(kUsbDevicePathSigFile))) {
    return false;
  }

  // Copy the public key into the target stateful for use in validation.
  auto public_key_on_device = device_config_dir.Append(oobe_config::kKeyFile);
  if (!base::CopyFile(public_key_file_, public_key_on_device)) {
    PLOG(ERROR) << "Failed to copy public key " << public_key_file_.value()
                << " to " << public_key_on_device.value();
    return false;
  }

  if (!ChangeDeviceOobeConfigDirOwnership()) {
    return false;
  }
  return true;
}

// Copied from rollback_helper.cc.
// TODO(ahassani): Find a way to use the one in rollback_helper.cc
bool SaveOobeConfigUsb::GetUidGid(const string& user,
                                  uid_t* uid,
                                  gid_t* gid) const {
  // Load the passwd entry.
  constexpr int kDefaultPwnameLength = 1024;
  int user_name_length = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (user_name_length == -1) {
    user_name_length = kDefaultPwnameLength;
  }
  if (user_name_length < 0) {
    return false;
  }
  passwd user_info{}, *user_infop;
  vector<char> user_name_buf(static_cast<size_t>(user_name_length));
  if (getpwnam_r(user.c_str(), &user_info, user_name_buf.data(),
                 static_cast<size_t>(user_name_length), &user_infop)) {
    return false;
  }
  *uid = user_info.pw_uid;
  *gid = user_info.pw_gid;
  return true;
}

bool SaveOobeConfigUsb::ChangeDeviceOobeConfigDirOwnership() const {
  // Find the GID/UID of oobe_config_restore.
  uid_t uid;
  gid_t gid;
  if (!GetUidGid(kOobeConfigRestoreUser, &uid, &gid)) {
    PLOG(ERROR) << "Failed to get the UID/GID for " << kOobeConfigRestoreUser;
    return false;
  }

  // We need to set the oobe_config_restore as the owner of the oobe_auto_config
  // directory and all files inside it so it can be read/deleted by the oobe
  // config service.
  auto device_config_dir = device_stateful_.Append(kUnencryptedOobeConfigDir);
  vector<FilePath> paths = {device_config_dir};
  FileEnumerator iter(device_config_dir, false, FileEnumerator::FILES);
  for (auto path = iter.Next(); !path.empty(); path = iter.Next()) {
    paths.emplace_back(path);
  }

  for (const auto& path : paths) {
    if (lchown(path.value().c_str(), uid, gid) != 0) {
      PLOG(ERROR) << "Couldn't change ownership of " << path.value() << " to "
                  << kOobeConfigRestoreUser;
      return false;
    }
  }
  return true;
}

bool SaveOobeConfigUsb::FindPersistentMountDevice(FilePath* device) const {
  base::FileEnumerator by_id(device_ids_dir_, false,
                             base::FileEnumerator::FILES);
  for (auto link = by_id.Next(); !link.empty(); link = by_id.Next()) {
    // |link| points to something like:
    //   usb-_Some_Memory_<serial>-0:0-part1 -> ../../sdb1
    FilePath target;
    if (!base::NormalizeFilePath(link, &target)) {
      // Do not fail if it doesn't normalize for some reason. We will fail at
      // the end if not found.
      PLOG(WARNING) << "Failed to normalize path " << link.value()
                    << "; Ignoring";
    }
    if (target == usb_device_) {
      LOG(INFO) << link.value() << " points to " << target.value();
      *device = link;
      return true;
    }
  }
  LOG(ERROR) << "Couldn't find persistent device mapping for "
             << usb_device_.value();
  return false;
}

void SaveOobeConfigUsb::Cleanup() const {
  auto device_config_dir = device_stateful_.Append(kUnencryptedOobeConfigDir);
  if (!DeleteFile(device_config_dir, true)) {
    PLOG(ERROR) << "Failed to delete directory: " << device_config_dir.value()
                << "; Just giving up!";
  }
}

bool SaveOobeConfigUsb::Save() const {
  if (!SaveInternal()) {
    LOG(ERROR) << "Failed to save the oobe_config files; cleaning up whatever"
               << " we created.";
    Cleanup();
    return false;
  }
  LOG(INFO) << "Saving OOBE config files was successful! hooray!";
  return true;
}

}  // namespace oobe_config

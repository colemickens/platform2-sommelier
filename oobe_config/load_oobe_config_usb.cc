// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_usb.h"

#include <sys/mount.h>

#include <map>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "oobe_config/usb_utils.h"

using base::FilePath;
using base::ScopedTempDir;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;

namespace oobe_config {

unique_ptr<LoadOobeConfigUsb> LoadOobeConfigUsb::CreateInstance() {
  return std::make_unique<LoadOobeConfigUsb>(FilePath(kStatefulDir),
                                             FilePath(kDevDiskById));
}

LoadOobeConfigUsb::LoadOobeConfigUsb(const FilePath& stateful_dir,
                                     const FilePath& device_ids_dir)
    : stateful_(stateful_dir),
      device_ids_dir_(device_ids_dir),
      public_key_(nullptr),
      config_is_verified_(false) {
  unencrypted_oobe_config_dir_ = stateful_.Append(kUnencryptedOobeConfigDir);
  pub_key_file_ = unencrypted_oobe_config_dir_.Append(kKeyFile);
  config_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kConfigFile).AddExtension("sig");
  enrollment_domain_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kDomainFile).AddExtension("sig");
  usb_device_path_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kUsbDevicePathSigFile);
}

bool LoadOobeConfigUsb::ReadFiles() {
  if (!base::PathExists(stateful_)) {
    LOG(ERROR) << "Stateful partition's path " << stateful_.value()
               << " does not exist.";
    return false;
  }
  if (!base::PathExists(unencrypted_oobe_config_dir_)) {
    LOG(WARNING) << "oobe_config directory on stateful partition "
                 << unencrypted_oobe_config_dir_.value()
                 << " does not exist. This is not an error if the system is not"
                 << " configured for auto oobe.";
    return false;
  }

  if (!base::PathExists(pub_key_file_)) {
    LOG(WARNING) << "Public key file " << pub_key_file_.value()
                 << " does not exist. ";
    return false;
  }

  if (!ReadPublicKey(pub_key_file_, &public_key_)) {
    return false;
  }

  map<FilePath, string*> signature_files = {
      {config_signature_file_, &config_signature_},
      {enrollment_domain_signature_file_, &enrollment_domain_signature_},
      {usb_device_path_signature_file_, &usb_device_path_signature_}};

  for (auto& file : signature_files) {
    if (!base::PathExists(file.first)) {
      LOG(WARNING) << "File " << file.first.value() << " does not exist. ";
      return false;
    }
    if (!base::ReadFileToString(file.first, file.second)) {
      PLOG(ERROR) << "Failed to read file: " << file.first.value();
      return false;
    }
  }

  return true;
}

bool LoadOobeConfigUsb::VerifyPublicKey() {
  // TODO(ahassani): calculate the SHA256 of the public key and return false if
  // doesn't match the one in TPM.
  // /stateful/unencrypted/oobe_auto_config/validation_key.pub is the key used
  // for verifying signatures. The binary SHA256 digest of this file should
  // match the 32 bytes in the TPM at index 0x100c, otherwise abort.

  return false;
}

bool LoadOobeConfigUsb::LocateUsbDevice(FilePath* device_id) {
  // /stateful/unencrypted/oobe_auto_config/usb_device_path.sig is the signature
  // of a /dev/disk/by-id path for the root USB device (e.g. /dev/sda). So to
  // obtain which USB we were on pre-reboot:
  // for dev in /dev/disk/by-id/ *:
  //     if dev verifies with usb_device_path.sig with validation_key.pub:
  //       USB block device is at readlink(dev)
  base::FileEnumerator iter(device_ids_dir_, false,
                            base::FileEnumerator::FILES);
  for (auto link = iter.Next(); !link.empty(); link = iter.Next()) {
    if (VerifySignature(link.value(), usb_device_path_signature_,
                        public_key_) &&
        base::NormalizeFilePath(link, device_id)) {
      LOG(INFO) << "Found USB device " << device_id->value();
      return true;
    }
  }

  LOG(ERROR) << "Did not find the USB device. Probably it was taken out?";
  return false;
}

bool LoadOobeConfigUsb::VerifyEnrollmentDomainInConfig(
    const string& config, const string& enrollment_domain) {
  // TODO(ahassani): Implement this.
  return true;
}

bool LoadOobeConfigUsb::MountUsbDevice(const FilePath& device_path,
                                       const FilePath& mount_point) {
  LOG(INFO) << "Mounting " << device_path.value() << " on "
            << mount_point.value();
  auto kMountFlags = MS_RDONLY | MS_NOEXEC | MS_NOSUID | MS_NODEV;
  if (mount(device_path.value().c_str(), mount_point.value().c_str(), "ext4",
            kMountFlags, nullptr) != 0) {
    PLOG(ERROR) << "Failed to mount " << device_path.value() << "on "
                << mount_point.value();
    return false;
  }

  return true;
}

bool LoadOobeConfigUsb::UnmountUsbDevice(const FilePath& mount_point) {
  LOG(INFO) << "Unmounting " << mount_point.value();
  if (umount(mount_point.value().c_str()) != 0) {
    PLOG(WARNING) << "Failed to unmount " << mount_point.value();
    return false;
  }
  return true;
}

bool LoadOobeConfigUsb::GetOobeConfigJson(string* config,
                                          string* enrollment_domain) {
  // We have already verified the config, just return it.
  CHECK(config != nullptr);
  CHECK(enrollment_domain != nullptr);
  if (config_is_verified_) {
    *config = config_;
    *enrollment_domain = enrollment_domain_;
    return true;
  }

  if (!ReadFiles()) {
    return false;
  }

  if (!VerifyPublicKey()) {
    return false;
  }

  // By now we have all the files necessary on the stateful partition. Now we
  // have to look into the USB drives.
  FilePath device_path;
  if (!LocateUsbDevice(&device_path)) {
    return false;
  }

  ScopedTempDir usb_mount_path;
  if (!usb_mount_path.CreateUniqueTempDir() ||
      !MountUsbDevice(device_path, usb_mount_path.GetPath())) {
    return false;
  }

  // /stateful/unencrypted/oobe_auto_config/config.json.sig is the signature of
  // the config.json file on the USB stateful.
  FilePath unencrypted_oobe_config_dir_on_usb =
      usb_mount_path.GetPath().Append(kUnencryptedOobeConfigDir);
  FilePath config_file_on_usb =
      unencrypted_oobe_config_dir_on_usb.Append(kConfigFile);
  if (!base::ReadFileToString(config_file_on_usb, &config_)) {
    LOG(ERROR) << "Failed to read oobe config file: "
               << config_file_on_usb.value();
    return false;
  }
  if (!VerifySignature(config_, config_signature_, public_key_)) {
    return false;
  }

  // /stateful/unencrypted/oobe_auto_config/enrollment_domain.sig is the
  // signature of the enrollment_domain file on the USB stateful.
  FilePath enrollment_domain_file_on_usb =
      unencrypted_oobe_config_dir_on_usb.Append(kDomainFile);
  if (!base::ReadFileToString(enrollment_domain_file_on_usb,
                              &enrollment_domain_)) {
    LOG(ERROR) << "Failed to read enrollment domain file: "
               << enrollment_domain_file_on_usb.value();
    return false;
  }
  if (!VerifySignature(enrollment_domain_, enrollment_domain_signature_,
                       public_key_)) {
    return false;
  }

  // The config.json has only an enrollment token, not a plain text domain, so
  // at some point before enrolling we have to verify that the token in
  // config.json matches the domain name in enrollment_domain, because that's
  // what we showed to the user in recovery.
  if (!VerifyEnrollmentDomainInConfig(config_, enrollment_domain_)) {
    return false;
  }

  *config = config_;
  *enrollment_domain = enrollment_domain_;
  config_is_verified_ = true;

  // Ignore the failure.
  UnmountUsbDevice(usb_mount_path.GetPath());
  return true;
}

}  // namespace oobe_config

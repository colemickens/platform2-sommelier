// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_usb.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>

#include "oobe_config/usb_common.h"

using base::FilePath;
using std::string;
using std::unique_ptr;

namespace oobe_config {

unique_ptr<LoadOobeConfigUsb> LoadOobeConfigUsb::CreateInstance() {
  return std::make_unique<LoadOobeConfigUsb>(FilePath(kStatefulDir),
                                             FilePath(kDevDiskById));
}

LoadOobeConfigUsb::LoadOobeConfigUsb(const FilePath& stateful_dir,
                                     const FilePath& device_ids_dir)
    : stateful_(stateful_dir), device_ids_dir_(device_ids_dir) {
  unencrypted_oobe_config_dir_ = stateful_.Append(kUnencryptedOobeConfigDir);
  pub_key_file_ = unencrypted_oobe_config_dir_.Append(kKeyFile);
  config_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kConfigFile).AddExtension("sig");
  enrollment_domain_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kDomainFile).AddExtension("sig");
  usb_device_path_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kUsbDevicePathSigFile);
}

bool LoadOobeConfigUsb::CheckFilesExistence() {
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
  if (!base::PathExists(config_signature_file_)) {
    LOG(WARNING) << "Config file's signature file "
                 << config_signature_file_.value() << " does not exist.";
    return false;
  }
  if (!base::PathExists(enrollment_domain_signature_file_)) {
    LOG(WARNING) << "Enrollment domain's signature file "
                 << enrollment_domain_signature_file_.value()
                 << " does not exist.";
    return false;
  }
  if (!base::PathExists(usb_device_path_signature_file_)) {
    LOG(WARNING) << "USB device path's signature file "
                 << usb_device_path_signature_file_.value()
                 << " does not exist.";
    return false;
  }

  return true;
}

bool LoadOobeConfigUsb::VerifyPublicKey() {
  // TODO(ahassani): calculate the SHA256 of the public key and return false if
  // doesn't match the one in TPM.
  // /stateful/unencrypted/oobe_auto_config/validation_key.pub is the key used
  // for verifying signatures. The binary SHA256 digest of this file should
  // match the 32 bytes in the TPM at index 0x100c, otherwise abort.

  // TODO(ahassani): Implement.

  return false;
}

bool LoadOobeConfigUsb::VerifySignature(const FilePath& file,
                                        const FilePath& signature) {
  // TODO(ahassani): Implement.
  return false;
}

bool LoadOobeConfigUsb::LocateUsbDevice(FilePath* device_id) {
  // /stateful/unencrypted/oobe_auto_config/usb_device_path.sig is the signature
  // of a /dev/disk/by-id path for the root USB device (e.g. /dev/sda). So to
  // obtain which USB we were on pre-reboot,
  // for dev in /dev/disk/by-id/ *:
  //     if dev verifies with usb_device_path.sig with validation_key.pub:
  //         (subprocess OpenSSL for this) USB block device is at readlink(dev)

  base::FileEnumerator iter(device_ids_dir_, false,
                            base::FileEnumerator::FILES);
  for (auto dev_id = iter.Next(); !dev_id.empty(); dev_id = iter.Next()) {
    if (VerifySignature(dev_id, usb_device_path_signature_file_)) {
      // Found the device; do a readlink -f and return the absolute path to the
      // device.
      // TODO(ahassani): Implement.
      return true;
    }
  }

  LOG(ERROR) << "Did not find the USB device. Probably it was taken out?";
  return false;
}

bool LoadOobeConfigUsb::VerifyEnrollmentDomainInConfig(
    const string& config, const string& enrollment_domain) {
  // TODO(ahassani): Implement this.
  return false;
}

bool LoadOobeConfigUsb::MountUsbDevice(const FilePath& device_path,
                                       const FilePath& mount_point) {
  // TODO(ahassani): Implement.
  return false;
}

bool LoadOobeConfigUsb::UnmountUsbDevice(const FilePath& mount_point) {
  // TODO(ahassani): Implement.
  return false;
}

bool LoadOobeConfigUsb::GetOobeConfigJson(string* config,
                                          string* enrollment_domain) {
  if (!CheckFilesExistence()) {
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

  FilePath usb_mount_path;  // Make some random directory.
  if (!MountUsbDevice(device_path, usb_mount_path)) {
    return false;
  }

  FilePath config_file_on_usb;
  FilePath enrollment_domain_file_on_usb;
  // /stateful/unencrypted/oobe_auto_config/config.json.sig is the signature of
  // the config.json file on the USB stateful.
  if (!VerifySignature(config_file_on_usb, config_signature_file_)) {
    return false;
  }

  // /stateful/unencrypted/oobe_auto_config/enrollment_domain.sig is the
  // signature of the enrollment_domain file on the USB stateful.
  if (!VerifySignature(enrollment_domain_file_on_usb,
                       enrollment_domain_signature_file_)) {
    return false;
  }

  if (!base::ReadFileToString(config_file_on_usb, config)) {
    LOG(ERROR) << "Failed to read oobe config file: "
               << config_file_on_usb.value();
    return false;
  }

  if (!base::ReadFileToString(enrollment_domain_file_on_usb,
                              enrollment_domain)) {
    LOG(ERROR) << "Failed to read enrollment domain file: "
               << enrollment_domain_file_on_usb.value();
    return false;
  }

  // The config.json has only an enrollment token, not a plain text domain, so
  // at some point before enrolling we have to verify that the token in
  // config.json matches the domain name in enrollment_domain, because that's
  // what we showed to the user in recovery.
  if (!VerifyEnrollmentDomainInConfig(*config, *enrollment_domain)) {
    return false;
  }

  if (!UnmountUsbDevice(usb_mount_path)) {
    // TODO(ahassani): Ignore the failure; But log it.
  }

  return true;
}

}  // namespace oobe_config

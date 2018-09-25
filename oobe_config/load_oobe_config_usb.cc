// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/load_oobe_config_usb.h"

#include <map>
#include <vector>

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include "oobe_config/usb_common.h"
#include "oobe_config/utils.h"

using base::FilePath;
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
    : stateful_(stateful_dir), device_ids_dir_(device_ids_dir) {
  unencrypted_oobe_config_dir_ = stateful_.Append(kUnencryptedOobeConfigDir);
  pub_key_file_ = unencrypted_oobe_config_dir_.Append(kKeyFile);
  config_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kConfigFile).AddExtension("sig");
  enrollment_domain_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kDomainFile).AddExtension("sig");
  usb_device_path_signature_file_ =
      unencrypted_oobe_config_dir_.Append(kUsbDevicePathSigFile);

  config_is_verified_ = false;
  public_key_ = nullptr;
}

bool LoadOobeConfigUsb::ReadFiles(bool ignore_errors) {
  if (!base::PathExists(stateful_) && !ignore_errors) {
    LOG(ERROR) << "Stateful partition's path " << stateful_.value()
               << " does not exist.";
    return false;
  }
  if (!base::PathExists(unencrypted_oobe_config_dir_) && !ignore_errors) {
    LOG(WARNING) << "oobe_config directory on stateful partition "
                 << unencrypted_oobe_config_dir_.value()
                 << " does not exist. This is not an error if the system is not"
                 << " configured for auto oobe.";
    return false;
  }

  if (!base::PathExists(pub_key_file_) && !ignore_errors) {
    LOG(WARNING) << "Public key file " << pub_key_file_.value()
                 << " does not exist. ";
    return false;
  }

  if (!ReadPublicKey() && !ignore_errors) {
    return false;
  }

  map<FilePath, string*> signature_files = {
      {config_signature_file_, &config_signature_},
      {enrollment_domain_signature_file_, &enrollment_domain_signature_},
      {usb_device_path_signature_file_, &usb_device_path_signature_}};

  for (auto& file : signature_files) {
    if (!base::PathExists(file.first) && !ignore_errors) {
      LOG(WARNING) << "File " << file.first.value() << " does not exist. ";
      return false;
    }
    if (!base::ReadFileToString(file.first, file.second) && !ignore_errors) {
      PLOG(ERROR) << "Failed to read file: " << file.first.value();
      return false;
    }
  }

  return true;
}

bool LoadOobeConfigUsb::ReadPublicKey() {
  base::ScopedFILE pkf(base::OpenFile(pub_key_file_, "r"));
  if (!pkf) {
    PLOG(ERROR) << "Failed to open the public key file "
                << pub_key_file_.value();
    return false;
  }
  public_key_.reset(PEM_read_PUBKEY(pkf.get(), nullptr, nullptr, nullptr));
  if (!public_key_) {
    LOG(ERROR) << "Failed to read the PEM public key file "
               << pub_key_file_.value();
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

  return false;
}

bool LoadOobeConfigUsb::VerifySignature(const string& message,
                                        const string& signature) {
  crypto::ScopedEVP_MD_CTX mdctx(EVP_MD_CTX_create());
  if (!mdctx) {
    LOG(ERROR) << "Failed to create a EVP_MD context.";
    return false;
  }
  if (EVP_DigestVerifyInit(mdctx.get(), nullptr, EVP_sha256(), nullptr,
                           public_key_.get()) != 1 ||
      EVP_DigestVerifyUpdate(mdctx.get(), message.c_str(), message.length()) !=
          1 ||
      EVP_DigestVerifyFinal(
          mdctx.get(), reinterpret_cast<const unsigned char*>(signature.data()),
          signature.size()) != 1) {
    LOG(ERROR) << "Failed to verify the signature.";
    return false;
  }
  return true;
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
    if (VerifySignature(dev_id.value(), usb_device_path_signature_)) {
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
  // We have already verified the config, just return it.
  if (config_is_verified_) {
    *config = config_;
    *enrollment_domain = enrollment_domain_;
    return true;
  }

  if (!ReadFiles(false /* ignore_errors */)) {
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
  if (!base::ReadFileToString(config_file_on_usb, &config_)) {
    LOG(ERROR) << "Failed to read oobe config file: "
               << config_file_on_usb.value();
    return false;
  }

  if (!base::ReadFileToString(enrollment_domain_file_on_usb,
                              &enrollment_domain_)) {
    LOG(ERROR) << "Failed to read enrollment domain file: "
               << enrollment_domain_file_on_usb.value();
    return false;
  }

  // /stateful/unencrypted/oobe_auto_config/config.json.sig is the signature of
  // the config.json file on the USB stateful.
  if (!VerifySignature(config_, config_signature_)) {
    return false;
  }

  // /stateful/unencrypted/oobe_auto_config/enrollment_domain.sig is the
  // signature of the enrollment_domain file on the USB stateful.
  if (!VerifySignature(enrollment_domain_, enrollment_domain_signature_)) {
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

  if (!UnmountUsbDevice(usb_mount_path)) {
    // TODO(ahassani): Ignore the failure; But log it.
  }

  return true;
}

}  // namespace oobe_config

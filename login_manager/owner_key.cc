// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/owner_key.h"

#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/crypto/signature_verifier.h>

#include "login_manager/system_utils.h"

namespace login_manager {

// Note: this structure is an ASN.1 which encodes the algorithm used
// with its parameters. This is defined in PKCS #1 v2.1 (RFC 3447).
// It is encoding: { OID sha1WithRSAEncryption      PARAMETERS NULL }
// static
const uint8 OwnerKey::kAlgorithm[15] = {
  0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
  0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
};

OwnerKey::OwnerKey(const FilePath& key_file)
    : key_file_(key_file),
      have_checked_disk_(false) {
}

OwnerKey::~OwnerKey() {}

bool OwnerKey::HaveCheckedDisk() { return have_checked_disk_; }

bool OwnerKey::IsPopulated() { return !key_.empty(); }

bool OwnerKey::PopulateFromDiskIfPossible() {
  have_checked_disk_ = true;
  if (!file_util::PathExists(key_file_)) {
    LOG(INFO) << "No owner key on disk.";
    return true;
  }

  SystemUtils utils;
  int32 safe_file_size = 0;
  if (!utils.EnsureAndReturnSafeFileSize(key_file_, &safe_file_size)) {
    LOG(ERROR) << key_file_.value() << " is too large!";
    return false;
  }

  key_.resize(safe_file_size);
  int data_read = file_util::ReadFile(key_file_,
                                      reinterpret_cast<char*>(&key_[0]),
                                      safe_file_size);
  if (data_read != safe_file_size) {
    PLOG(ERROR) << key_file_.value() << " could not be read in its entirety!";
    key_.resize(0);
    return false;
  }
  return true;
}

bool OwnerKey::PopulateFromBuffer(const std::vector<uint8>& public_key_der) {
  if (!HaveCheckedDisk()) {
    LOG(WARNING) << "Haven't checked disk for owner key yet!";
    return false;
  }
  // Only get here if we've checked disk already.
  if (IsPopulated()) {
    LOG(ERROR) << "Already have an owner key!";
    return false;
  }
  // Only get here if we've checked disk AND we didn't load a key.
  key_ = public_key_der;
  return true;
}

bool OwnerKey::Persist() {
  // It is a programming error to call this before checking for the key on disk.
  CHECK(have_checked_disk_) << "Haven't checked disk for owner key yet!";
  if (file_util::PathExists(key_file_)) {
    LOG(ERROR) << "Tried to overwrite owner key!";
    return false;
  }

  if (key_.size() != file_util::WriteFile(key_file_,
                                          reinterpret_cast<char*>(&key_[0]),
                                          key_.size())) {
    PLOG(ERROR) << "Could not write data to " << key_file_.value();
    return false;
  }
  return true;
}

bool OwnerKey::Verify(const char* data,
                      uint32 data_len,
                      const char* signature,
                      uint32 sig_len) {
  if (!verifier_.VerifyInit(kAlgorithm,
                            sizeof(kAlgorithm),
                            reinterpret_cast<const uint8*>(signature),
                            sig_len,
                            &key_[0],
                            key_.size())) {
    LOG(ERROR) << "Could not initialize verifier";
    return false;
  }

  verifier_.VerifyUpdate(reinterpret_cast<const uint8*>(data), data_len);
  return (verifier_.VerifyFinal());
}

}  // namespace login_manager

// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/owner_key.h"

#include <base/crypto/rsa_private_key.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>

#include "login_manager/child_job.h"
#include "login_manager/nss_util.h"
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
      have_checked_disk_(false),
      have_replaced_(false),
      utils_(new SystemUtils) {
}

OwnerKey::~OwnerKey() {}

bool OwnerKey::Equals(const std::string& key_der) const {
  return VEquals(std::vector<uint8>(key_der.c_str(),
                                    key_der.c_str() + key_der.length()));
}

bool OwnerKey::VEquals(const std::vector<uint8>& key_der) const {
  return ((key_.empty() == key_der.empty()) &&
          memcmp(&key_der[0], &key_[0], key_.size()) == 0);
}

bool OwnerKey::HaveCheckedDisk() const { return have_checked_disk_; }

bool OwnerKey::IsPopulated() const { return !key_.empty(); }

bool OwnerKey::PopulateFromDiskIfPossible() {
  have_checked_disk_ = true;
  if (!file_util::PathExists(key_file_)) {
    LOG(INFO) << "No owner key on disk.";
    return true;
  }

  int32 safe_file_size = 0;
  if (!utils_->EnsureAndReturnSafeFileSize(key_file_, &safe_file_size)) {
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

bool OwnerKey::PopulateFromKeypair(base::RSAPrivateKey* pair) {
  std::vector<uint8> public_key_der;
  if (pair && pair->ExportPublicKey(&public_key_der))
    return PopulateFromBuffer(public_key_der);
  LOG(ERROR) << "Failed to export public key from key pair";
  return false;
}

bool OwnerKey::Persist() {
  // It is a programming error to call this before checking for the key on disk.
  CHECK(have_checked_disk_) << "Haven't checked disk for owner key yet!";
  if (!have_replaced_ && file_util::PathExists(key_file_)) {
    LOG(ERROR) << "Tried to overwrite owner key!";
    return false;
  }

  if (!utils_->AtomicFileWrite(key_file_,
                               reinterpret_cast<char*>(&key_[0]),
                               key_.size())) {
    PLOG(ERROR) << "Could not write data to " << key_file_.value();
    return false;
  }
  DLOG(INFO) << "wrote " << key_.size() << " bytes to " << key_file_.value();
  return true;
}

bool OwnerKey::Rotate(const std::vector<uint8>& public_key_der,
                      const std::vector<uint8>& signature) {
  if (!IsPopulated()) {
    LOG(ERROR) << "Don't yet have an owner key!";
    return false;
  }
  if (Verify(&public_key_der[0],
             public_key_der.size(),
             &signature[0],
             signature.size())) {
    key_ = public_key_der;
    have_replaced_ = true;
    return true;
  }
  LOG(ERROR) << "Invalid signature on new key!";
  return false;
}

bool OwnerKey::ClobberCompromisedKey(const std::vector<uint8>& public_key_der) {
  // It is a programming error to call this before checking for the key on disk.
  CHECK(have_checked_disk_) << "Haven't checked disk for owner key yet!";
  // It is a programming error to call this without a key already loaded.
  CHECK(IsPopulated()) << "Don't yet have an owner key!";

  key_ = public_key_der;
  return have_replaced_ = true;
}

bool OwnerKey::Verify(const uint8* data,
                      uint32 data_len,
                      const uint8* signature,
                      uint32 sig_len) {
  scoped_ptr<NssUtil> util(NssUtil::Create());
  if (!util->Verify(kAlgorithm,
                    sizeof(kAlgorithm),
                    signature,
                    sig_len,
                    data,
                    data_len,
                    &key_[0],
                    key_.size())) {
    LOG(ERROR) << "Signature verification of " << data << " failed";
    return false;
  }
  return true;
}

bool OwnerKey::Sign(const uint8* data,
                    uint32 data_len,
                    std::vector<uint8>* OUT_signature) {
  scoped_ptr<NssUtil> util(NssUtil::Create());
  scoped_ptr<base::RSAPrivateKey> private_key(util->GetPrivateKey(key_));
  if (!private_key.get())
    return false;
  if (!util->Sign(data, data_len, OUT_signature, private_key.get())) {
    LOG(ERROR) << "Signing of " << data << " failed";
    return false;
  }
  return true;
}

int OwnerKey::StartGeneration(ChildJobInterface* generator) {
  int pid = fork();
  if (pid == 0) {
    generator->Run();
    exit(1);  // Run() is not supposed to return.
  }
  return pid;
}

}  // namespace login_manager

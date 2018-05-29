// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/fake_le_credential_backend.h"

#include <openssl/sha.h>

#include "cryptohome/cryptolib.h"
#include "fake_le_credential_metadata.pb.h"  // NOLINT(build/include)

namespace cryptohome {

// Initial root hash when the leaf length is 14 bits, and each node
// has 4 children.
const uint8_t kInitRootHash14_4[SHA256_DIGEST_LENGTH] = {
    0x91, 0x3C, 0xA7, 0x20, 0x82, 0x23, 0xB8, 0xC8, 0x92, 0xA6, 0x1E,
    0x83, 0xD9, 0x68, 0x07, 0x28, 0xE3, 0xE1, 0xD6, 0xBB, 0x10, 0x63,
    0xF2, 0xDD, 0xCE, 0x92, 0x25, 0x71, 0x80, 0x3D, 0xA9, 0xEE};

FakeLECredentialBackend::FakeLECredentialBackend() {
  fake_root_hash_.reserve(SHA256_DIGEST_LENGTH);
}

bool FakeLECredentialBackend::Reset(std::vector<uint8_t>* new_root) {
  fake_root_hash_ = std::vector<uint8_t>(
      kInitRootHash14_4, kInitRootHash14_4 + SHA256_DIGEST_LENGTH);
  *new_root = fake_root_hash_;
  return true;
}

bool FakeLECredentialBackend::InsertCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const brillo::SecureBlob& le_secret,
    const brillo::SecureBlob& he_secret,
    const brillo::SecureBlob& reset_secret,
    const std::map<uint32_t, uint32_t>& delay_schedule,
    std::vector<uint8_t>* cred_metadata,
    std::vector<uint8_t>* mac,
    std::vector<uint8_t>* new_root) {
  // Calculate the root hash, assuming new MAC is 32 bytes of 0.
  std::vector<uint8_t> leaf_mac(SHA256_DIGEST_LENGTH, 0);
  // Set |new_root| to the original value, in case we return errors.
  *new_root = fake_root_hash_;

  std::vector<uint8_t> root_hash = RecalculateRootHash(label, leaf_mac, h_aux);
  if (root_hash != fake_root_hash_) {
    LOG(ERROR) << "h_aux and/or metadata don't match the current root hash.";
    return false;
  }

  // Generate the credential metadata structure.
  // TODO(pmalani): Still have to add support for the delay schedule.
  // The "encryption" of the credential is just a NOP.
  FakeLECredentialMetadata cred_metadata_entry;
  cred_metadata_entry.set_label(label);
  cred_metadata_entry.set_le_secret(le_secret.data(), le_secret.size());
  cred_metadata_entry.set_he_secret(he_secret.data(), he_secret.size());
  cred_metadata_entry.set_reset_secret(reset_secret.data(),
                                       reset_secret.size());
  cred_metadata_entry.set_attempt_count(0);

  cred_metadata->resize(cred_metadata_entry.ByteSize());
  if (!cred_metadata_entry.SerializeToArray(cred_metadata->data(),
                                            cred_metadata->size())) {
    LOG(ERROR) << "Couldn't serialize cred metadata, label: " << label;
    return false;
  }

  // Actual TPM will actually calculate the MAC, but for testing,
  // just a SHA should be sufficient.
  *mac = CryptoLib::Sha256(*cred_metadata);

  fake_root_hash_ = RecalculateRootHash(label, *mac, h_aux);
  *new_root = fake_root_hash_;

  return true;
}

bool FakeLECredentialBackend::CheckCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const std::vector<uint8_t>& orig_cred_metadata,
    const brillo::SecureBlob& le_secret,
    std::vector<uint8_t>* new_cred_metadata,
    std::vector<uint8_t>* new_mac,
    brillo::SecureBlob* he_secret,
    LECredBackendError* err,
    std::vector<uint8_t>* new_root) {
  *err = LE_TPM_SUCCESS;
  new_cred_metadata->clear();
  new_mac->clear();
  // Set |new_root| to the original value, in case we return errors.
  *new_root = fake_root_hash_;

  std::vector<uint8_t> orig_mac = CryptoLib::Sha256(orig_cred_metadata);

  std::vector<uint8_t> root_hash = RecalculateRootHash(label, orig_mac, h_aux);
  if (root_hash != fake_root_hash_) {
    LOG(ERROR) << "h_aux and/or metadata don't match the current root hash.";
    *err = LE_TPM_ERROR_HASH_TREE_SYNC;
    return false;
  }

  FakeLECredentialMetadata metadata_tmp;
  if (!metadata_tmp.ParseFromArray(orig_cred_metadata.data(),
                                   orig_cred_metadata.size())) {
    LOG(INFO) << "Couldn't deserialize cred metadata, label: " << label;
    *err = LE_TPM_ERROR_HASH_TREE_SYNC;
    return false;
  }

  if (metadata_tmp.attempt_count() >= LE_MAX_INCORRECT_ATTEMPTS) {
    *err = LE_TPM_ERROR_TOO_MANY_ATTEMPTS;
    return false;
  }

  // Check the LE secret.
  if (!memcmp(metadata_tmp.le_secret().data(), le_secret.data(),
              le_secret.size())) {
    metadata_tmp.set_attempt_count(0);
    he_secret->assign(metadata_tmp.he_secret().begin(),
                      metadata_tmp.he_secret().end());
  } else {
    *err = LE_TPM_ERROR_INVALID_LE_SECRET;
    metadata_tmp.set_attempt_count(metadata_tmp.attempt_count() + 1);
  }

  new_cred_metadata->resize(metadata_tmp.ByteSize());
  if (!metadata_tmp.SerializeToArray(new_cred_metadata->data(),
                                     new_cred_metadata->size())) {
    LOG(ERROR) << "Couldn't serialize new cred metadata, label: " << label;
    new_cred_metadata->clear();
    return false;
  }

  *new_mac = CryptoLib::Sha256(*new_cred_metadata);

  fake_root_hash_ = RecalculateRootHash(label, *new_mac, h_aux);
  *new_root = fake_root_hash_;

  return (*err == LE_TPM_SUCCESS);
}

bool FakeLECredentialBackend::ResetCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const std::vector<uint8_t>& orig_cred_metadata,
    const brillo::SecureBlob& reset_secret,
    std::vector<uint8_t>* new_cred_metadata,
    std::vector<uint8_t>* new_mac,
    LECredBackendError* err,
    std::vector<uint8_t>* new_root) {
  *err = LE_TPM_SUCCESS;
  new_cred_metadata->clear();
  new_mac->clear();
  // Set |new_root| to the original value, in case we return errors.
  *new_root = fake_root_hash_;

  std::vector<uint8_t> orig_mac = CryptoLib::Sha256(orig_cred_metadata);

  std::vector<uint8_t> root_hash = RecalculateRootHash(label, orig_mac, h_aux);
  if (root_hash != fake_root_hash_) {
    LOG(ERROR) << "h_aux and/or metadata don't match the current root hash.";
    *err = LE_TPM_ERROR_HASH_TREE_SYNC;
    return false;
  }

  FakeLECredentialMetadata metadata_tmp;
  if (!metadata_tmp.ParseFromArray(orig_cred_metadata.data(),
                                   orig_cred_metadata.size())) {
    LOG(INFO) << "Couldn't deserialize cred metadata, label: " << label;
    *err = LE_TPM_ERROR_HASH_TREE_SYNC;
    return false;
  }

  // Check the Reset secret.
  if (!memcmp(metadata_tmp.reset_secret().data(), reset_secret.data(),
              reset_secret.size())) {
    metadata_tmp.set_attempt_count(0);
  } else {
    *err = LE_TPM_ERROR_INVALID_RESET_SECRET;
  }

  new_cred_metadata->resize(metadata_tmp.ByteSize());
  if (!metadata_tmp.SerializeToArray(new_cred_metadata->data(),
                                     new_cred_metadata->size())) {
    LOG(ERROR) << "Couldn't serialize new cred metadata, label: " << label;
    new_cred_metadata->clear();
    return false;
  }

  *new_mac = CryptoLib::Sha256(*new_cred_metadata);

  fake_root_hash_ = RecalculateRootHash(label, *new_mac, h_aux);
  *new_root = fake_root_hash_;

  return (*err == LE_TPM_SUCCESS);
}

bool FakeLECredentialBackend::RemoveCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const std::vector<uint8_t>& mac,
    std::vector<uint8_t>* new_root) {
  // Set |new_root| to the original value, in case we return errors.
  *new_root = fake_root_hash_;
  std::vector<uint8_t> root_hash = RecalculateRootHash(label, mac, h_aux);

  if (root_hash != fake_root_hash_) {
    LOG(ERROR) << "h_aux and/or metadata don't match the current root hash.";
    return false;
  }

  // Create a new MAC which is all zeros.
  brillo::SecureBlob new_mac(SHA256_DIGEST_LENGTH, 0);
  fake_root_hash_ = RecalculateRootHash(label, new_mac, h_aux);
  *new_root = fake_root_hash_;

  return true;
}

bool FakeLECredentialBackend::GetLog(
    const std::vector<uint8_t>& cur_disk_root_hash,
    std::vector<uint8_t>* root_hash) {
  root_hash->assign(fake_root_hash_.begin(), fake_root_hash_.end());
  // TODO(b/809710): Mimick the log functionality of PinWeaver.
  return true;
}

std::vector<uint8_t> FakeLECredentialBackend::RecalculateRootHash(
    const uint64_t label,
    const std::vector<uint8_t>& leaf_mac,
    const std::vector<std::vector<uint8_t>>& h_aux) {
  std::vector<uint8_t> cur_hash = leaf_mac;
  uint64_t cur_label = label;
  auto it = h_aux.begin();

  uint32_t height = kLengthLabels / kBitsPerLevel;
  while (height--) {
    std::vector<uint8_t> input_buffer;
    // Get bottom kBitsPerLevel bits of current label.
    uint64_t cur_suffix = cur_label & ((1 << kBitsPerLevel) - 1);
    // Go from left through right for all the sibling nodes. If we encounter the
    // suffix for the current node, add it to the input buffer, otherwise add
    // the sibling node.
    for (uint64_t i = 0; i < kNumChildren; i++) {
      if (i == cur_suffix) {
        input_buffer.insert(input_buffer.end(), cur_hash.begin(),
                            cur_hash.end());
      } else {
        auto sibling_hash = *it;
        input_buffer.insert(input_buffer.end(), sibling_hash.begin(),
                            sibling_hash.end());
        ++it;
      }
    }
    std::vector<uint8_t> result_hash = CryptoLib::Sha256(input_buffer);
    cur_hash = result_hash;
    cur_label = cur_label >> kBitsPerLevel;
  }

  return cur_hash;
}

}  // namespace cryptohome

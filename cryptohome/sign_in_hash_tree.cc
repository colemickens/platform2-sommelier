// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/sign_in_hash_tree.h"

#include <fcntl.h>

#include <utility>

#include <base/files/file_util.h>
#include <brillo/secure_blob.h>

#include "cryptohome/cryptolib.h"

namespace {
const char kHashCacheFileName[] = "hashcache";
}

namespace cryptohome {

SignInHashTree::SignInHashTree(uint32_t leaf_length,
                               uint8_t bits_per_level,
                               base::FilePath basedir)
    : leaf_length_(leaf_length),
      fan_out_(1 << bits_per_level),
      bits_per_level_(bits_per_level),
      p_(new Platform()),
      plt_(p_.get(), basedir) {
  // leaf_length_ should be divisible by bits_per_level_.
  CHECK(!(leaf_length_ % bits_per_level_));

  // TODO(pmalani): This should not happen on cryptohomed restart.
  plt_.InitOnBoot();

  // The number of entries in the hash tree can be given by the geometric
  // series: For a height H, the number of entries in the hash tree can be given
  // by the relation:
  //   num_entries(H) = num_entries(H-1) + fan_out^(H-1)
  //
  // This can be collapsed into the closed form expression:
  // num_entries(H) = (fan_out^(H + 1) - 1) / (fan_out - 1)
  uint32_t height = leaf_length_ / bits_per_level_;
  uint32_t num_entries =
      ((1 << (bits_per_level_ * (height + 1))) - 1) / (fan_out_ - 1);

  // Ensure a hash cache file of the right size exists, so that we can mmap it
  // correctly later.
  base::FilePath hash_cache_file = basedir.Append(kHashCacheFileName);
  auto hash_cache_fd = std::make_unique<base::ScopedFD>(open(
      hash_cache_file.value().c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR));
  CHECK(hash_cache_fd->is_valid());
  CHECK(!ftruncate(hash_cache_fd->get(), num_entries * 32));
  hash_cache_fd.reset();

  CHECK(hash_cache_.Initialize(hash_cache_file,
                               base::MemoryMappedFile::READ_WRITE));
  hash_cache_array_ = reinterpret_cast<uint8_t(*)[32]>(hash_cache_.data());
}

SignInHashTree::~SignInHashTree() {}

std::vector<SignInHashTree::Label> SignInHashTree::GetAuxiliaryLabels(
    const Label& leaf_label) {
  std::vector<Label> aux_labels;

  Label cur_label = leaf_label;
  while (!cur_label.is_root()) {
    Label parent = cur_label.GetParent();
    for (uint64_t i = 0; i < fan_out_; i++) {
      Label child = parent.Extend(i);
      if (child != cur_label) {
        aux_labels.push_back(child);
      }
    }
    cur_label = parent;
  }

  return aux_labels;
}

void SignInHashTree::GenerateAndStoreHashCache() {
  CalculateHash(Label(0, 0, bits_per_level_));
}

bool SignInHashTree::StoreLabel(const Label& label,
                                const std::vector<uint8_t>& hmac,
                                const std::vector<uint8_t>& cred_metadata) {
  if (IsLeafLabel(label)) {
    std::vector<uint8_t> merged_blob;

    merged_blob.reserve(hmac.size() + cred_metadata.size());
    merged_blob.insert(merged_blob.end(), hmac.begin(), hmac.end());
    merged_blob.insert(merged_blob.end(), cred_metadata.begin(),
                       cred_metadata.end());
    if (!plt_.StoreValue(label.value(), merged_blob)) {
      LOG(ERROR) << "Couldn't store label: " << label.value() << " in PLT.";
      return false;
    }
  }

  memcpy(hash_cache_array_[label.cache_index()], hmac.data(), 32);
  // TODO(pmalani): Probably have to update all the parent hashes here.
  return true;
}

bool SignInHashTree::RemoveLabel(const Label& label) {
  // Update the PLT if |label| is a leaf node.
  if (!IsLeafLabel(label)) {
    LOG(ERROR) << "Label provided is not for leaf node: " << label.value();
    return false;
  }

  if (!plt_.RemoveKey(label.value())) {
    LOG(ERROR) << "Couldn't remove label: " << label.value() << " in PLT.";
    return false;
  }

  std::vector<uint8_t> hmac(32, 0);
  memcpy(hash_cache_array_[label.cache_index()], hmac.data(), 32);
  // TODO(pmalani): Probably have to update all the parent hashes here.
  return true;
}

bool SignInHashTree::GetLabelData(const Label& label,
                                  std::vector<uint8_t>* hmac,
                                  std::vector<uint8_t>* cred_metadata) {
  // If it is a leaf node, just get all the data from the PLT directly.
  hmac->reserve(32);
  if (IsLeafLabel(label)) {
    std::vector<uint8_t> merged_blob;
    if (!plt_.GetValue(label.value(), &merged_blob)) {
      LOG(WARNING) << "Couldn't get key: " << label.value() << " in PLT.";
      // Assume that the label doesn't exist, so return all-zero HMAC.
      hmac->assign(32, 0);
      LOG(INFO) << "Returning HMAC 0 for empty key.";
      return true;
    }
    cred_metadata->reserve(merged_blob.size() - 32);
    hmac->assign(merged_blob.begin(), merged_blob.begin() + 32);
    cred_metadata->assign(merged_blob.begin() + 32, merged_blob.end());
  } else {
    // If it is a inner leaf, get the value from the HashCache file.
    hmac->assign(hash_cache_array_[label.cache_index()],
                 hash_cache_array_[label.cache_index()] + 32);
  }
  return true;
}

SignInHashTree::Label SignInHashTree::GetFreeLabel() {
  // Iterate through all the leaf nodes in the PLT and see if any key is valid.
  //
  // TODO(pmalani): This approach will lead to the labels bunching near
  // the start of the label namespace. This may be problematic when an
  // out-of-sync situation that only affects the first child of the root would
  // cause the entire tree to always go out of sync. Try to evenly space out the
  // distribution of labels.
  for (uint64_t i = 0; i < (1 << leaf_length_); i++) {
    if (!plt_.KeyExists(i)) {
      return Label(i, leaf_length_, bits_per_level_);
    }
  }
  return Label();
}

std::vector<uint8_t> SignInHashTree::CalculateHash(const Label& label) {
  std::vector<uint8_t> ret_val;
  if (IsLeafLabel(label)) {
    // This is a leaf node, so just get HMAC from the PersistentLookupTable and
    // store it here.
    std::vector<uint8_t> merged_blob;
    if (plt_.GetValue(label.value(), &merged_blob)) {
      std::vector<uint8_t> hmac;
      hmac.assign(merged_blob.begin(), merged_blob.begin() + 32);
      ret_val = hmac;
    } else {
      // If the PLT isn't available or the GetValue() failed, we assume an empty
      // credential and assign it an HMAC of 32 bytes of 0.
      std::vector<uint8_t> empty_hmac(32, 0);
      ret_val = empty_hmac;
    }
  } else {
    // Join all the child hashes / HMACs together, and hash the result.
    std::vector<uint8_t> input_buffer;
    for (uint64_t i = 0; i < fan_out_; i++) {
      Label child_label = label.Extend(i);
      std::vector<uint8_t> child_hash = CalculateHash(child_label);
      input_buffer.insert(input_buffer.end(), child_hash.begin(),
                          child_hash.end());
    }
    brillo::SecureBlob result_hash = CryptoLib::Sha256(input_buffer);
    ret_val = result_hash;
  }

  // Update the hash cache with the new value.
  memcpy(hash_cache_array_[label.cache_index()], ret_val.data(), 32);
  return ret_val;
}

}  // namespace cryptohome

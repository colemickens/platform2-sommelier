// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_TPM_UTILITY_IMPL_H
#define CHAPS_TPM_UTILITY_IMPL_H

#include "chaps/tpm_utility.h"

#include <map>
#include <set>
#include <string>

#include <base/basictypes.h>
#include <base/synchronization/lock.h>
#include <trousers/scoped_tss_type.h>
#include <trousers/tss.h>

namespace chaps {

class TPMUtilityImpl : public TPMUtility {
 public:
  TPMUtilityImpl(const std::string& srk_auth_data);
  virtual ~TPMUtilityImpl();
  virtual bool Init();
  virtual bool Authenticate(int slot_id,
                            const chromeos::SecureBlob& auth_data,
                            const std::string& auth_key_blob,
                            const std::string& encrypted_master_key,
                            chromeos::SecureBlob* master_key);
  virtual bool ChangeAuthData(int slot_id,
                              const chromeos::SecureBlob& old_auth_data,
                              const chromeos::SecureBlob& new_auth_data,
                              const std::string& old_auth_key_blob,
                              std::string* new_auth_key_blob);
  virtual bool GenerateRandom(int num_bytes, std::string* random_data);
  virtual bool StirRandom(const std::string& entropy_data);
  virtual bool GenerateKey(int slot,
                           int modulus_bits,
                           const std::string& public_exponent,
                           const chromeos::SecureBlob& auth_data,
                           std::string* key_blob,
                           int* key_handle);
  virtual bool GetPublicKey(int key_handle,
                            std::string* public_exponent,
                            std::string* modulus);
  virtual bool WrapKey(int slot,
                       const std::string& public_exponent,
                       const std::string& modulus,
                       const std::string& prime_factor,
                       const chromeos::SecureBlob& auth_data,
                       std::string* key_blob,
                       int* key_handle);
  virtual bool LoadKey(int slot,
                       const std::string& key_blob,
                       const chromeos::SecureBlob& auth_data,
                       int* key_handle);
  virtual bool LoadKeyWithParent(int slot,
                                 const std::string& key_blob,
                                 const chromeos::SecureBlob& auth_data,
                                 int parent_key_handle,
                                 int* key_handle);
  virtual void UnloadKeysForSlot(int slot);
  virtual bool Bind(int key_handle,
                    const std::string& input,
                    std::string* output);
  virtual bool Unbind(int key_handle,
                      const std::string& input,
                      std::string* output);
  virtual bool Sign(int key_handle,
                    const std::string& input,
                    std::string* signature);
  virtual bool Verify(int key_handle,
                      const std::string& input,
                      const std::string& signature);
  // Stringifies TSS error codes.
  static std::string ResultToString(TSS_RESULT result);
 private:
  // Holds handle information for each slot.
  struct HandleInfo {
    // The set of all handles (for the slot).
    std::set<int> handles_;
    // Maps known blobs to the associated key handle.
    std::map<std::string, int> blob_handle_;
  };

  // Holds key information for each key handle.
  struct KeyInfo {
    TSS_HKEY tss_handle;
    std::string blob;
    chromeos::SecureBlob auth_data;
  };

  int CreateHandle(int slot,
                   TSS_HKEY key,
                   const std::string& key_blob,
                   const chromeos::SecureBlob& auth_data);
  bool CreateKeyPolicy(TSS_HKEY key,
                       const chromeos::SecureBlob& auth_data,
                       bool auth_only);
  bool GetKeyAttributeData(TSS_HKEY key,
                           TSS_FLAG flag,
                           TSS_FLAG sub_flag,
                           std::string* data);
  bool GetKeyBlob(TSS_HKEY key, std::string* blob);
  TSS_FLAG GetKeyFlags(int modulus_bits);
  bool GetSRKPublicKey();
  TSS_HKEY GetTssHandle(int key_handle);
  bool IsAlreadyLoaded(int slot, const std::string& key_blob, int* key_handle);
  bool LoadKeyInternal(TSS_HKEY parent,
                       const std::string& key_blob,
                       const chromeos::SecureBlob& auth_data,
                       TSS_HKEY* key);
  bool ReloadKey(int key_handle);

  bool is_initialized_;
  trousers::ScopedTssContext tsp_context_;
  TSS_HPOLICY default_policy_;
  TSS_HKEY srk_;
  std::string srk_auth_data_;
  bool srk_public_loaded_;
  const std::string default_exponent_;
  std::map<int, HandleInfo> slot_handles_;
  std::map<int, KeyInfo> handle_info_;
  base::Lock lock_;
  int last_handle_;

  DISALLOW_COPY_AND_ASSIGN(TPMUtilityImpl);
};

}  // namespace

#endif  // CHAPS_TPM_UTILITY_IMPL_H

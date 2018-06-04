// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_FAKE_LE_CREDENTIAL_BACKEND_H_
#define CRYPTOHOME_FAKE_LE_CREDENTIAL_BACKEND_H_

#include <map>
#include <vector>

#include <base/files/file_util.h>

#include "cryptohome/le_credential_backend.h"

namespace cryptohome {

// TODO(pmalani): Get max attempts from delay schedule.
// Hard code max attempts at 5 for now.
const int LE_MAX_INCORRECT_ATTEMPTS = 5;

// Implementation of the LECredentialBackend interface. This class
// mimicks all the actual TPM-backed LECrdentialBackend functionality on
// the host side itself. It is useful for prototyping host side features,
// as well as for unit testing LECredentialManager.
//
// In lieu of NvRAM, we store the root hash in a 32-byte vector.
class FakeLECredentialBackend : public LECredentialBackend {
 public:
  FakeLECredentialBackend();
  bool Reset(std::vector<uint8_t>* new_root) override;

  // For the fake backend, we can always assume it's supported.
  bool IsSupported() override { return true; };

  bool InsertCredential(const uint64_t label,
                        const std::vector<std::vector<uint8_t>>& h_aux,
                        const brillo::SecureBlob& le_secret,
                        const brillo::SecureBlob& he_secret,
                        const brillo::SecureBlob& reset_secret,
                        const std::map<uint32_t, uint32_t>& delay_schedule,
                        std::vector<uint8_t>* cred_metadata,
                        std::vector<uint8_t>* mac,
                        std::vector<uint8_t>* new_root) override;

  bool CheckCredential(const uint64_t label,
                       const std::vector<std::vector<uint8_t>>& h_aux,
                       const std::vector<uint8_t>& orig_cred_metadata,
                       const brillo::SecureBlob& le_secret,
                       std::vector<uint8_t>* new_cred_metadata,
                       std::vector<uint8_t>* new_mac,
                       brillo::SecureBlob* he_secret,
                       LECredBackendError* err,
                       std::vector<uint8_t>* new_root) override;

  bool ResetCredential(const uint64_t label,
                       const std::vector<std::vector<uint8_t>>& h_aux,
                       const std::vector<uint8_t>& orig_cred_metadata,
                       const brillo::SecureBlob& reset_secret,
                       std::vector<uint8_t>* new_cred_metadata,
                       std::vector<uint8_t>* new_mac,
                       LECredBackendError* err,
                       std::vector<uint8_t>* new_root) override;

  bool RemoveCredential(const uint64_t label,
                        const std::vector<std::vector<uint8_t>>& h_aux,
                        const std::vector<uint8_t>& mac,
                        std::vector<uint8_t>* new_root) override;

 private:
  // Helper function to calculate root hash, given a leaf with label |label|,
  // MAC value |mac, and a set of auxiliary hashes |h_aux|.
  // Returns a 32-byte vector root hash as a result.
  std::vector<uint8_t> RecalculateRootHash(
      const uint64_t label,
      const std::vector<uint8_t>& leaf_mac,
      const std::vector<std::vector<uint8_t>>& h_aux);

  // Location where the fake backend stores the root hash.
  std::vector<uint8_t> fake_root_hash_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_FAKE_LE_CREDENTIAL_BACKEND_H_

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PINWEAVER_LE_CREDENTIAL_BACKEND_H_
#define CRYPTOHOME_PINWEAVER_LE_CREDENTIAL_BACKEND_H_

#include <map>
#include <string>
#include <vector>

#include <brillo/secure_blob.h>

#include "cryptohome/le_credential_backend.h"

namespace cryptohome {

class Tpm2Impl;

// Low-entropy credential backend implementation using PinWeaver running on Cr50
// and accessed via trunks.
class PinweaverLECredentialBackend : public LECredentialBackend {
 public:
  explicit PinweaverLECredentialBackend(Tpm2Impl* tpm);

  // LECredentialBackend
  bool Reset(std::vector<uint8_t>* new_root) override;
  bool IsSupported() override;
  bool InsertCredential(const uint64_t label,
                        const std::vector<std::vector<uint8_t>>& h_aux,
                        const brillo::SecureBlob& le_secret,
                        const brillo::SecureBlob& he_secret,
                        const brillo::SecureBlob& reset_secret,
                        const std::map<uint32_t, uint32_t>& delay_schedule,
                        const ValidPcrCriteria& valid_pcr_criteria,
                        std::vector<uint8_t>* cred_metadata,
                        std::vector<uint8_t>* mac,
                        std::vector<uint8_t>* new_root) override;
  bool NeedsPCRBinding(const std::vector<uint8_t>& cred_metadata) override;
  bool CheckCredential(const uint64_t label,
                       const std::vector<std::vector<uint8_t>>& h_aux,
                       const std::vector<uint8_t>& orig_cred_metadata,
                       const brillo::SecureBlob& le_secret,
                       std::vector<uint8_t>* new_cred_metadata,
                       std::vector<uint8_t>* new_mac,
                       brillo::SecureBlob* he_secret,
                       brillo::SecureBlob* reset_secret,
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
  bool GetLog(const std::vector<uint8_t>& cur_disk_root_hash,
              std::vector<uint8_t>* root_hash,
              std::vector<LELogEntry>* log) override;

  bool ReplayLogOperation(const std::vector<uint8_t>& log_root,
                          const std::vector<std::vector<uint8_t>>& h_aux,
                          const std::vector<uint8_t>& orig_cred_metadata,
                          std::vector<uint8_t>* new_cred_metadata,
                          std::vector<uint8_t>* new_mac) override;

 private:
  // Convenience wrapper implementing common boilerplate for all types of
  // PinWeaver operations. |name| is a descriptive name of the operation used in
  // log messages. |err| is the location to fill in with the error code and
  // might be nullptr if not needed. |op| implements the actual operation and
  // should be a function object (intended for use with a lambda expression) of
  // the following type:
  //
  //     std::pair<trunks::TPM_RC, in32_t>(trunks::TpmUtility*)
  //
  // Gets a trunks context and bails out if that fails, else executes |op|
  // passing the TpmUtility* from the context as a parameter. Expects |op| to
  // return a pair of TPM and PinWeaver status codes, which gets mapped to
  // LECredentialBackendError and stored in |err|. Returns whether the operation
  // was successful, i.e. both TPM and PinWeaver status codes indicate success.
  template <typename Operation>
  bool PerformPinweaverOperation(const std::string& name,
                                 LECredBackendError* err,
                                 Operation op);

  Tpm2Impl* tpm_;
  // The protocol version used by pinweaver.
  uint8_t protocol_version_ = 255;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PINWEAVER_LE_CREDENTIAL_BACKEND_H_

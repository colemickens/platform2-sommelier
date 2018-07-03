// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_LE_CREDENTIAL_BACKEND_H_
#define CRYPTOHOME_LE_CREDENTIAL_BACKEND_H_

#include <map>
#include <string>
#include <vector>

#include <brillo/secure_blob.h>

namespace cryptohome {

// Constants used to define the hash tree. Currently we place them here,
// since they are used between LECredentialManager and LECredentialBackend.
const uint32_t kLengthLabels = 14;
const uint32_t kNumChildren = 4;
const uint32_t kBitsPerLevel = 2;

// List of error values returned from TPM for the Low Entropy Credential
// check routine.
enum LECredBackendError {
  // Credential check was successful.
  LE_TPM_SUCCESS = 0,
  // Check failed due to incorrect Low Entropy credential provided.
  LE_TPM_ERROR_INVALID_LE_SECRET,
  // Reset failed due to incorrect Reset credential provided.
  LE_TPM_ERROR_INVALID_RESET_SECRET,
  // Check failed since the credential has been locked out due to too many
  // attempts per the delay schedule.
  LE_TPM_ERROR_TOO_MANY_ATTEMPTS,
  // Check failed due to the hash tree being out of sync. This should
  // prompt a hash tree resynchronization and retry.
  LE_TPM_ERROR_HASH_TREE_SYNC,
  // Check failed due to an operation failing on the TPM side. This should
  // prompt a hash tree resynchronization and retry.
  LE_TPM_ERROR_TPM_OP_FAILED,
  // The PCR is in unexpected state. The basic way to proceed from here is to
  // reboot the device.
  LE_TPM_ERROR_PCR_NOT_MATCH,
};

// Enum used to denote the LE log entry type.
enum LELogEntryType {
  LE_LOG_INSERT = 0,
  LE_LOG_CHECK,
  LE_LOG_REMOVE,
  LE_LOG_RESET,
  // Sentinel value.
  LE_LOG_INVALID,
};

// Container for LE credential log replay data obtained from the LE Backend.
// This struct is used during sychronization operations which occur when the
// on-disk hash tree state and LE Backend hash tree state are out-of-sync.
struct LELogEntry {
  LELogEntryType type;
  // Label on which the log operation is performed.
  uint64_t label;
  // Value of root hash after the log operation is performed.
  std::vector<uint8_t> root;
  // For insert operations, this signifies the MAC of the inserted leaf.
  std::vector<uint8_t> mac;
};

// Defines a set of PCR indexes (in bitmask) and the digest that is valid
// after computation of sha256 of concatenation of PCR values included in
// bitmask.
struct ValidPcrValue {
  /* The set of PCR indexes that have to pass the validation. */
  uint8_t bitmask[2];
  /* The hash digest of the PCR values contained in the bitmask */
  std::string digest;
};

typedef std::vector<ValidPcrValue> ValidPcrCriteria;

// LECredentialBackend - class for performing Low Entropy (LE) Credential
// related operations in the TPM. The Tpm class implementations which support LE
// Credential handling will contain an object of a class which implements
// this interface. The base Tpm Class will have a functnon which can be
// used to retrieve a reference to this object. For Tpm implementations which
// don't support LE Credentials, the aforementioned function will return a
// nullptr.
class LECredentialBackend {
 public:
  // Resets the TPM Low Entropy (LE) Credential Hash Tree root hash
  // with its initial known value, which assumes all MACs are all-zero.
  //
  // This function should be executed only when we are setting up a hash tree
  // on a new / wiped device, or resetting the hash tree due to an
  // unrecoverable error.
  //
  // Returns true on success, false otherwise.
  //
  // In all cases, the resulting root hash is returned in |new_root|.
  virtual bool Reset(std::vector<uint8_t>* new_root) = 0;

  // Returns whether LE credential protection is supported in this specific
  // backend. Not all TPM2-based h/w will support this feature (only Cr50
  // and later), so this function will only return true for h/w which does.
  virtual bool IsSupported() = 0;

  // Tries to insert a credential into the TPM.
  //
  // The label of the leaf node is in |label|, the list of auxiliary hashes is
  // in |h_aux|, the LE credential to be added is in |le_secret|. Along with
  // it, its associated reset_secret |reset_secret| and the high entropy
  // credential it protects |he_secret| are also provided. The delay schedule
  // which determines the delay enforced between authentication attempts is
  // provided by |delay_schedule|. The list of valid PCR values that would be
  // accepted by authentication is provided by |valid_pcr_criteria|.
  //
  // If successful, the new credential metadata will be placed in the blob
  // pointed by |cred_metadata|. The MAC of the credential will be returned
  // in |mac|.
  //
  // Returns true on success, false on failure.
  //
  // |h_aux| requires a particular order: starting from left child to right
  // child, from leaf upwards till the children of the root label.
  //
  // In all cases, the resulting root hash is returned in |new_root|.
  virtual bool InsertCredential(
      const uint64_t label,
      const std::vector<std::vector<uint8_t>>& h_aux,
      const brillo::SecureBlob& le_secret,
      const brillo::SecureBlob& he_secret,
      const brillo::SecureBlob& reset_secret,
      const std::map<uint32_t, uint32_t>& delay_schedule,
      const ValidPcrCriteria& valid_pcr_criteria,
      std::vector<uint8_t>* cred_metadata,
      std::vector<uint8_t>* mac,
      std::vector<uint8_t>* new_root) = 0;

  // Checks the metadata leaf version and returns if the leaf needs to be bound
  // to PCR.
  virtual bool NeedsPCRBinding(const std::vector<uint8_t>& cred_metadata) = 0;

  // Tries to verify/authenticate a credential.
  //
  // The obfuscated LE Credential is |le_secret| and the credential metadata is
  // in |orig_cred_metadata|.
  //
  // Returns true on success, false on failure.
  // On success, |err| is set to LE_TPM_SUCCESS.
  // On failure, |err| is set with the appropriate error code:
  // - LE_TPM_ERROR_INVALID_LE_SECRET for incorrect authentication attempt.
  // - LE_TPM_ERROR_TOO_MANY_ATTEMPTS for locked out credential.
  // - LE_TPM_ERROR_HASH_TREE for error in hash tree sync.
  // - LE_TPM_ERROR_TPM_OP_FAILED for Tpm operation error.
  //
  // On success, or failure due to an invalid |le_secret|, the updated
  // credential metadata and corresponding new MAC will be returned in
  // |new_cred_metadata| and |new_mac|.
  //
  // On success, the released high entropy credential will be returned in
  // |he_secret| and the reset secret in |reset_secret|.
  //
  // In all cases, the resulting root hash is returned in |new_root|.
  virtual bool CheckCredential(const uint64_t label,
                               const std::vector<std::vector<uint8_t>>& h_aux,
                               const std::vector<uint8_t>& orig_cred_metadata,
                               const brillo::SecureBlob& le_secret,
                               std::vector<uint8_t>* new_cred_metadata,
                               std::vector<uint8_t>* new_mac,
                               brillo::SecureBlob* he_secret,
                               brillo::SecureBlob* reset_secret,
                               LECredBackendError* err,
                               std::vector<uint8_t>* new_root) = 0;

  // Tries to reset a (potentially locked out) credential.
  //
  // The reset credential is |reset_secret| and the credential metadata is
  // in |orig_cred_metadata|.
  //
  // Returns true on success, false on failure.
  // On success, |err| is set to LE_TPM_SUCCESS.
  // On failure, |err| is set with the appropriate error code:
  // - LE_TPM_ERROR_INVALID_RESET_SECRET for incorrect authentication attempt.
  // - LE_TPM_ERROR_HASH_TREE for error in hash tree sync.
  // - LE_TPM_ERROR_TPM_OP_FAILED for Tpm operation error.
  //
  // On success, the updated credential metadata and corresponding new MAC will
  // be returned in |new_cred_metadata| and |new_mac|.
  //
  // In all cases, the resulting root hash is returned in |new_root|.
  virtual bool ResetCredential(const uint64_t label,
                               const std::vector<std::vector<uint8_t>>& h_aux,
                               const std::vector<uint8_t>& orig_cred_metadata,
                               const brillo::SecureBlob& reset_secret,
                               std::vector<uint8_t>* new_cred_metadata,
                               std::vector<uint8_t>* new_mac,
                               LECredBackendError* err,
                               std::vector<uint8_t>* new_root) = 0;

  // Removes the credential which has label |label|.
  //
  // The corresponding list of auxiliary hashes is in |h_aux|, and the MAC of
  // the label that needs to be removed is |mac|.
  //
  // Returns true on success, false on failure.
  //
  // In all cases, the resulting root hash is returned in |new_root|.
  virtual bool RemoveCredential(const uint64_t label,
                                const std::vector<std::vector<uint8_t>>& h_aux,
                                const std::vector<uint8_t>& mac,
                                std::vector<uint8_t>* new_root) = 0;

  // Retrieves the replay log.
  //
  // The current on-disk root hash is supplied via |cur_disk_root_hash|.
  // The LE backend's current root hash is returned in |root_hash|
  //
  // Returns true on success (was able to communicate with the backend), and
  // false otherwise.
  // TODO(crbug.com/809710): Add a |log_data| vector which should return log
  // entries in already-parsed form.
  virtual bool GetLog(const std::vector<uint8_t>& cur_disk_root_hash,
                      std::vector<uint8_t>* root_hash,
                      std::vector<LELogEntry>* log) = 0;

  // Replays the log operation referenced by |log_entry_root|, where
  // |log_entry_root| is the resulting root hash after the operation, and is
  // retrieved from the log entry.
  // |h_aux| and |orig_cred_metadata| refer to, respectively, the list of
  // auxiliary hashes and the original credential metadata associated with the
  // label concerned (available in the log entry). The resulting metadata and
  // MAC are stored in |new_cred_metadata| and |new_mac|.
  //
  // Returns true on success, false on failure.
  virtual bool ReplayLogOperation(
      const std::vector<uint8_t>& log_entry_root,
      const std::vector<std::vector<uint8_t>>& h_aux,
      const std::vector<uint8_t>& orig_cred_metadata,
      std::vector<uint8_t>* new_cred_metadata,
      std::vector<uint8_t>* new_mac) = 0;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_LE_CREDENTIAL_BACKEND_H_

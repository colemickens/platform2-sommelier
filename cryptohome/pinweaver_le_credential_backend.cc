// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/pinweaver_le_credential_backend.h"

#include <stdint.h>  // required by pinweaver_types.h

#include <utility>

#include <base/logging.h>

#include <trunks/error_codes.h>
#include <trunks/tpm_utility.h>
#include <trunks/trunks_factory.h>

extern "C" {
#define __packed __attribute((packed))
#define __aligned(x) __attribute((aligned(x)))
#include <trunks/cr50_headers/pinweaver_types.h>
}

#include "cryptohome/tpm2_impl.h"

namespace cryptohome {

namespace {

// Translates a pair of trunks error code and pinweaver status code to the
// appropriate LECredBackendError.
LECredBackendError ConvertStatus(const std::string& operation,
                                 trunks::TPM_RC result,
                                 int32_t pinweaver_status) {
  if (result != trunks::TPM_RC_SUCCESS) {
    LOG(ERROR) << "TPM error on pinweaver " << operation
               << " operation: " << trunks::GetErrorString(result);
    return LE_TPM_ERROR_TPM_OP_FAILED;
  }

  // TODO(808688): This is useful to diagnose pinweaver behavior in the field.
  // Remove once pinweaver is mature enough so the benefit does no longer
  // justify the log spam.
  LOG(INFO) << "Pinweaver " << operation << ": " << pinweaver_status;

  switch (pinweaver_status) {
    case 0:  // EC_SUCCESS
      return LE_TPM_SUCCESS;
    case PW_ERR_LOWENT_AUTH_FAILED:
      return LE_TPM_ERROR_INVALID_LE_SECRET;
    case PW_ERR_RESET_AUTH_FAILED:
      return LE_TPM_ERROR_INVALID_RESET_SECRET;
    case PW_ERR_RATE_LIMIT_REACHED:
      return LE_TPM_ERROR_TOO_MANY_ATTEMPTS;
    case PW_ERR_PATH_AUTH_FAILED:
      return LE_TPM_ERROR_HASH_TREE_SYNC;
  }

  LOG(ERROR) << "Pinweaver error on pinweaver " << operation
             << " operation: " << pinweaver_status;
  return LE_TPM_ERROR_TPM_OP_FAILED;
}

std::string EncodeAuxHashes(const std::vector<std::vector<uint8_t>>& h_aux) {
  std::string result;
  result.reserve(h_aux.size() * PW_HASH_SIZE);
  for (const std::vector<uint8_t>& hash : h_aux) {
    CHECK_EQ(PW_HASH_SIZE, hash.size());
    result.append(hash.begin(), hash.end());
  }
  return result;
}

std::vector<uint8_t> StringToBlob(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

std::string BlobToString(const std::vector<uint8_t>& blob) {
  return std::string(blob.begin(), blob.end());
}

}  // namespace

PinweaverLECredentialBackend::PinweaverLECredentialBackend(Tpm2Impl* tpm)
    : tpm_(tpm) {}

bool PinweaverLECredentialBackend::Reset(std::vector<uint8_t>* new_root) {
  return PerformPinweaverOperation(
      "Reset", nullptr, [&](trunks::TpmUtility* tpm_utility) {
        uint32_t pinweaver_status;
        std::string root;
        trunks::TPM_RC result = tpm_utility->PinWeaverResetTree(
            kBitsPerLevel, kLengthLabels / kBitsPerLevel, &pinweaver_status,
            &root);
        *new_root = StringToBlob(root);
        return std::make_pair(result, pinweaver_status);
      });
}

bool PinweaverLECredentialBackend::IsSupported() {
  return PerformPinweaverOperation(
      "IsSupported", nullptr, [&](trunks::TpmUtility* tpm_utility) {
        return std::make_pair(tpm_utility->PinWeaverIsSupported(),
                              0 /* EC_SUCCESS */);
      });
}

bool PinweaverLECredentialBackend::InsertCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const brillo::SecureBlob& le_secret,
    const brillo::SecureBlob& he_secret,
    const brillo::SecureBlob& reset_secret,
    const std::map<uint32_t, uint32_t>& delay_schedule,
    std::vector<uint8_t>* cred_metadata,
    std::vector<uint8_t>* mac,
    std::vector<uint8_t>* new_root) {
  return PerformPinweaverOperation(
      "InsertCredential", nullptr, [&](trunks::TpmUtility* tpm_utility) {
        uint32_t pinweaver_status;
        std::string root;
        std::string cred_metadata_string;
        std::string mac_string;
        trunks::TPM_RC result = tpm_utility->PinWeaverInsertLeaf(
            label, EncodeAuxHashes(h_aux), le_secret, he_secret, reset_secret,
            delay_schedule, &pinweaver_status, &root, &cred_metadata_string,
            &mac_string);

        *cred_metadata = StringToBlob(cred_metadata_string);
        *mac = StringToBlob(mac_string);
        *new_root = StringToBlob(root);
        return std::make_pair(result, pinweaver_status);
      });
}

bool PinweaverLECredentialBackend::CheckCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const std::vector<uint8_t>& orig_cred_metadata,
    const brillo::SecureBlob& le_secret,
    std::vector<uint8_t>* new_cred_metadata,
    std::vector<uint8_t>* new_mac,
    brillo::SecureBlob* he_secret,
    LECredBackendError* err,
    std::vector<uint8_t>* new_root) {
  return PerformPinweaverOperation(
      "CheckCredential", err, [&](trunks::TpmUtility* tpm_utility) {
        uint32_t pinweaver_status;
        std::string root;
        uint32_t seconds_to_wait;
        std::string cred_metadata_string;
        std::string mac_string;
        trunks::TPM_RC result = tpm_utility->PinWeaverTryAuth(
            le_secret, EncodeAuxHashes(h_aux), BlobToString(orig_cred_metadata),
            &pinweaver_status, &root, &seconds_to_wait, he_secret,
            &cred_metadata_string, &mac_string);

        *new_cred_metadata = StringToBlob(cred_metadata_string);
        *new_mac = StringToBlob(mac_string);
        *new_root = StringToBlob(root);
        return std::make_pair(result, pinweaver_status);
      });
}

bool PinweaverLECredentialBackend::ResetCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const std::vector<uint8_t>& orig_cred_metadata,
    const brillo::SecureBlob& reset_secret,
    std::vector<uint8_t>* new_cred_metadata,
    std::vector<uint8_t>* new_mac,
    LECredBackendError* err,
    std::vector<uint8_t>* new_root) {
  return PerformPinweaverOperation(
      "ResetCredential", err, [&](trunks::TpmUtility* tpm_utility) {
        uint32_t pinweaver_status;
        std::string root;
        brillo::SecureBlob he_secret;
        std::string cred_metadata_string;
        std::string mac_string;
        trunks::TPM_RC result = tpm_utility->PinWeaverResetAuth(
            reset_secret, EncodeAuxHashes(h_aux),
            BlobToString(orig_cred_metadata), &pinweaver_status, &root,
            &he_secret, &cred_metadata_string, &mac_string);

        *new_cred_metadata = StringToBlob(cred_metadata_string);
        *new_mac = StringToBlob(mac_string);
        *new_root = StringToBlob(root);
        return std::make_pair(result, pinweaver_status);
      });
}

bool PinweaverLECredentialBackend::RemoveCredential(
    const uint64_t label,
    const std::vector<std::vector<uint8_t>>& h_aux,
    const std::vector<uint8_t>& mac,
    std::vector<uint8_t>* new_root) {
  return PerformPinweaverOperation(
      "RemoveCredential", nullptr, [&](trunks::TpmUtility* tpm_utility) {
        uint32_t pinweaver_status;
        std::string root;
        trunks::TPM_RC result = tpm_utility->PinWeaverRemoveLeaf(
            label, EncodeAuxHashes(h_aux), BlobToString(mac), &pinweaver_status,
            &root);
        *new_root = StringToBlob(root);
        return std::make_pair(result, pinweaver_status);
      });
}

bool PinweaverLECredentialBackend::GetLog(
    const std::vector<uint8_t>& cur_disk_root_hash,
    std::vector<uint8_t>* root_hash) {
  return PerformPinweaverOperation(
      "GetLog", nullptr, [&](trunks::TpmUtility* tpm_utility) {
        uint32_t pinweaver_status;
        std::string cur_root = BlobToString(cur_disk_root_hash);
        std::string root;
        std::vector<trunks::PinWeaverLogEntry> log;
        trunks::TPM_RC result = tpm_utility->PinWeaverGetLog(
            cur_root, &pinweaver_status, &root, &log);
        *root_hash = StringToBlob(root);
        // TODO(b/809710): Fix the handling/conversion of log data.
        return std::make_pair(result, pinweaver_status);
      });
}

template <typename Operation>
bool PinweaverLECredentialBackend::PerformPinweaverOperation(
    const std::string& name, LECredBackendError* err, Operation op) {
  Tpm2Impl::TrunksClientContext* trunks = nullptr;
  if (!tpm_->GetTrunksContext(&trunks)) {
    LOG(ERROR) << "Error getting trunks context for " << name;
    return false;
  }

  std::pair<trunks::TPM_RC, int32_t> status = op(trunks->tpm_utility.get());

  LECredBackendError error = ConvertStatus(name, status.first, status.second);
  if (err) {
    *err = error;
  }

  return error == LE_TPM_SUCCESS;
}

}  // namespace cryptohome

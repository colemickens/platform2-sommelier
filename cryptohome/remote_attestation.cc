// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_attestation.h"

#include <string>

#include <base/file_util.h>
#include <chromeos/secure_blob.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "cryptolib.h"
#include "remote_attestation.pb.h"
#include "tpm.h"

using std::string;

namespace cryptohome {

const size_t RemoteAttestation::kQuoteExternalDataSize = 20;
const size_t RemoteAttestation::kCipherKeySize = 32;
const size_t RemoteAttestation::kCipherBlockSize = 16;
const char* RemoteAttestation::kDefaultDatabasePath =
    "/home/.shadow/attestation.epb";

bool RemoteAttestation::IsPreparedForEnrollment() {
  base::AutoLock lock(prepare_lock_);
  if (!is_prepared_) {
    EncryptedDatabase encrypted_db;
    AttestationDatabase db;
    if (!LoadDatabase(&encrypted_db)) {
      LOG(INFO) << "Remote Attestation: Attestation data not found.";
      return false;
    }
    if (!DecryptDatabase(encrypted_db, &db)) {
      LOG(ERROR) << "Remote Attestation: Attestation data invalid.";
      return false;
    }
    LOG(INFO) << "Remote Attestation: Valid attestation data exists.";
    is_prepared_ = true;
  }
  return is_prepared_;
}

void RemoteAttestation::PrepareForEnrollment() {
  // If there is no TPM, we have no work to do.
  if (!tpm_)
    return;
  if (IsPreparedForEnrollment())
    return;
  LOG(INFO) << "Remote Attestation: Initializing...";
  chromeos::SecureBlob ek_public_key;
  if (!tpm_->GetEndorsementPublicKey(&ek_public_key)) {
    LOG(ERROR) << "Remote Attestation: Failed to get EK public key.";
    return;
  }
  // Create an AIK.
  chromeos::SecureBlob identity_public_key;
  chromeos::SecureBlob identity_key_blob;
  chromeos::SecureBlob identity_binding;
  chromeos::SecureBlob identity_label;
  chromeos::SecureBlob pca_public_key;
  chromeos::SecureBlob endorsement_credential;
  chromeos::SecureBlob platform_credential;
  chromeos::SecureBlob conformance_credential;
  if (!tpm_->MakeIdentity(&identity_public_key, &identity_key_blob,
                          &identity_binding, &identity_label, &pca_public_key,
                          &endorsement_credential, &platform_credential,
                          &conformance_credential)) {
    LOG(ERROR) << "Remote Attestation: Failed to make AIK.";
    return;
  }

  // Quote PCR0.
  chromeos::SecureBlob external_data;
  if (!tpm_->GetRandomData(kQuoteExternalDataSize, &external_data)) {
    LOG(ERROR) << "Remote Attestation: GetRandomData failed.";
    return;
  }
  chromeos::SecureBlob quoted_pcr_value;
  chromeos::SecureBlob quoted_data;
  chromeos::SecureBlob quote;
  if (!tpm_->QuotePCR0(identity_key_blob, external_data, &quoted_pcr_value,
                       &quoted_data, &quote)) {
    LOG(ERROR) << "Remote Attestation: Failed to generate quote.";
    return;
  }

  // Assemble a protobuf to store locally.
  base::AutoLock lock(prepare_lock_);
  AttestationDatabase database_pb;
  TPMCredentials* credentials_pb = database_pb.mutable_credentials();
  credentials_pb->set_endorsement_public_key(ek_public_key.data(),
                                             ek_public_key.size());
  credentials_pb->set_endorsement_credential(endorsement_credential.data(),
                                             endorsement_credential.size());
  credentials_pb->set_platform_credential(platform_credential.data(),
                                          platform_credential.size());
  credentials_pb->set_conformance_credential(conformance_credential.data(),
                                             conformance_credential.size());
  IdentityKey* key_pb = database_pb.mutable_identity_key();
  key_pb->set_identity_public_key(identity_public_key.data(),
                                  identity_public_key.size());
  key_pb->set_identity_key_blob(identity_key_blob.data(),
                                identity_key_blob.size());
  IdentityBinding* binding_pb = database_pb.mutable_identity_binding();
  binding_pb->set_identity_binding(identity_binding.data(),
                                   identity_binding.size());
  binding_pb->set_identity_public_key(identity_public_key.data(),
                                      identity_public_key.size());
  binding_pb->set_identity_label(identity_label.data(), identity_label.size());
  binding_pb->set_pca_public_key(pca_public_key.data(), pca_public_key.size());
  Quote* quote_pb = database_pb.mutable_pcr0_quote();
  quote_pb->set_quote(quote.data(), quote.size());
  quote_pb->set_quoted_data(quoted_data.data(), quoted_data.size());
  quote_pb->set_quoted_pcr_value(quoted_pcr_value.data(),
                                 quoted_pcr_value.size());
  if (!tpm_->GetRandomData(kCipherKeySize, &database_key_)) {
    LOG(ERROR) << "Remote Attestation: GetRandomData failed.";
    return;
  }
  chromeos::SecureBlob sealed_key;
  if (!tpm_->SealToPCR0(database_key_, &sealed_key)) {
    LOG(ERROR) << "Remote Attestation: Failed to seal cipher key.";
    return;
  }
  EncryptedDatabase encrypted_pb;
  encrypted_pb.set_sealed_key(sealed_key.data(), sealed_key.size());
  if (!EncryptDatabase(database_pb, &encrypted_pb)) {
    LOG(ERROR) << "Remote Attestation: Failed to encrypt db.";
    return;
  }
  if (!StoreDatabase(encrypted_pb)) {
    LOG(ERROR) << "Remote Attestation: Failed to store db.";
    return;
  }
  LOG(INFO) << "Remote Attestation: Initialization successful.";
  tpm_->RemoveOwnerDependency(Tpm::kRemoteAttestation);
}

void RemoteAttestation::PrepareForEnrollmentAsync() {
  // TODO(dkrahn): Implement this.
  PrepareForEnrollment();
}

bool RemoteAttestation::EncryptDatabase(const AttestationDatabase& db,
                                        EncryptedDatabase* encrypted_db) {
  chromeos::SecureBlob iv;
  if (!tpm_->GetRandomData(kCipherBlockSize, &iv)) {
    LOG(ERROR) << "GetRandomData failed.";
    return false;
  }
  string serial_string;
  if (!db.SerializeToString(&serial_string)) {
    LOG(ERROR) << "Failed to serialize db.";
    return false;
  }
  chromeos::SecureBlob serial_data(serial_string.data(), serial_string.size());
  chromeos::SecureBlob encrypted_data;
  if (!CryptoLib::AesEncrypt(serial_data, database_key_, iv, &encrypted_data)) {
    LOG(ERROR) << "Failed to encrypt db.";
    return false;
  }
  encrypted_db->set_encrypted_data(encrypted_data.data(),
                                   encrypted_data.size());
  encrypted_db->set_iv(iv.data(), iv.size());
  encrypted_db->set_mac(ComputeHMAC(*encrypted_db));
  return true;
}

bool RemoteAttestation::DecryptDatabase(const EncryptedDatabase& encrypted_db,
                                        AttestationDatabase* db) {
  chromeos::SecureBlob sealed_key(encrypted_db.sealed_key().data(),
                                  encrypted_db.sealed_key().length());
  if (!tpm_->Unseal(sealed_key, &database_key_)) {
    LOG(ERROR) << "Cannot unseal database key.";
    return false;
  }
  string mac = ComputeHMAC(encrypted_db);
  if (mac.length() != encrypted_db.mac().length()) {
    LOG(ERROR) << "Corrupted database.";
    return false;
  }
  if (0 != chromeos::SafeMemcmp(mac.data(), encrypted_db.mac().data(),
                                mac.length())) {
    LOG(ERROR) << "Corrupted database.";
    return false;
  }
  chromeos::SecureBlob iv(encrypted_db.iv().data(), encrypted_db.iv().length());
  chromeos::SecureBlob encrypted_data(encrypted_db.encrypted_data().data(),
                                      encrypted_db.encrypted_data().length());
  chromeos::SecureBlob serial_db;
  if (!CryptoLib::AesDecrypt(encrypted_data, database_key_, iv, &serial_db)) {
    LOG(ERROR) << "Failed to decrypt database.";
    return false;
  }
  if (!db->ParseFromArray(serial_db.data(), serial_db.size())) {
    LOG(ERROR) << "Failed to parse database.";
    return false;
  }
  return true;
}

string RemoteAttestation::ComputeHMAC(const EncryptedDatabase& encrypted_db) {
  chromeos::SecureBlob hmac_input(encrypted_db.iv().length() +
                                  encrypted_db.encrypted_data().length());
  memcpy(&hmac_input[0], encrypted_db.iv().data(), encrypted_db.iv().length());
  memcpy(&hmac_input[encrypted_db.iv().length()],
         encrypted_db.encrypted_data().data(),
         encrypted_db.encrypted_data().length());
  chromeos::SecureBlob hmac = CryptoLib::HmacSha512(database_key_, hmac_input);
  return string(reinterpret_cast<char*>(hmac.data()), hmac.size());
}

bool RemoteAttestation::StoreDatabase(const EncryptedDatabase& encrypted_db) {
  string database_serial;
  if (!encrypted_db.SerializeToString(&database_serial)) {
    LOG(ERROR) << "Failed to serialize encrypted db.";
    return false;
  }
  if (-1 == file_util::WriteFile(database_path_, database_serial.data(),
                                 database_serial.length())) {
    LOG(ERROR) << "Failed to write db.";
    return false;
  }
  return true;
}

bool RemoteAttestation::LoadDatabase(EncryptedDatabase* encrypted_db) {
  string serial;
  if (!file_util::ReadFileToString(database_path_, &serial)) {
    return false;
  }
  if (!encrypted_db->ParseFromString(serial)) {
    LOG(ERROR) << "Failed to parse encrypted db.";
    return false;
  }
  return true;
}

}

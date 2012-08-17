// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation.h"

#include <algorithm>
#include <string>

#include <arpa/inet.h>
#include <base/file_util.h>
#include <base/time.h>
#include <chromeos/secure_blob.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/x509.h>

#include "attestation.pb.h"
#include "cryptolib.h"
#include "tpm.h"

using chromeos::SecureBlob;
using std::string;

namespace cryptohome {

const size_t Attestation::kQuoteExternalDataSize = 20;
const size_t Attestation::kCipherKeySize = 32;
const size_t Attestation::kCipherBlockSize = 16;
const size_t Attestation::kNonceSize = 20;  // As per TPM_NONCE definition.
const size_t Attestation::kDigestSize = 20;  // As per TPM_DIGEST definition.
const char* Attestation::kDefaultDatabasePath =
    "/home/.shadow/attestation.epb";

const Attestation::CertificateAuthority Attestation::kKnownEndorsementCA[] = {
  {"IFX TPM EK Intermediate CA 06",
   "de9e58a353313d21d683c687d6aaaab240248717557c077161c5e515f41d8efa"
   "48329f45658fb550f43f91d1ba0c2519429fb6ef964f89657098c90a9783ad6d"
   "3baea625db044734c478768db53b6022c556d8174ed744bd6e4455665715cd5c"
   "beb7c3fcb822ab3dfab1ecee1a628c3d53f6085983431598fb646f04347d5ae0"
   "021d5757cc6e3027c1e13f10633ae48bbf98732c079c17684b0db58bd0291add"
   "e277b037dd13fa3db910e81a4969622a79c85ac768d870f079b54c2b98c856e7"
   "15ef0ba9c01ee1da1241838a1307fe94b1ddfa65cdf7eeaa7e5b4b8a94c3dcd0"
   "29bb5ebcfc935e56641f4c8cb5e726c68f9dd6b41f8602ef6dc78d870a773571"},
  {"NTC TPM EK Root CA 01",
   "e836ac61b43e3252d5e1a8a4061997a6a0a272ba3d519d6be6360cc8b4b79e8c"
   "d53c07a7ce9e9310ca84b82bbdad32184544ada357d458cf224c4a3130c97d00"
   "4933b5db232d8b6509412eb4777e9e1b093c58b82b1679c84e57a6b218b4d61f"
   "6dd4c3a66b2dd33b52cb1ffdff543289fa36dd71b7c83b66c1aae37caf7fe88d"
   "851a3523e3ea92b59a6b0ca095c5e1d191484c1bff8a33048c3976e826d4c12a"
   "e198f7199d183e0e70c8b46e8106edec3914397e051ae2b9a7f0b4bb9cd7f2ed"
   "f71064eb0eb473df27b7ccef9a018d715c5fe6ab012a8315f933c7f4fc35d34c"
   "efc27de224b2e3de3b3ba316d5df8b90b2eb879e219d270141b78dbb671a3a05"},
  {"STM TPM EK Intermediate CA 03",
   "a5152b4fbd2c70c0c9a0dd919f48ddcde2b5c0c9988cff3b04ecd844f6cc0035"
   "6c4e01b52463deb5179f36acf0c06d4574327c37572292fcd0f272c2d45ea7f2"
   "2e8d8d18aa62354c279e03be9220f0c3822d16de1ea1c130b59afc56e08f22f1"
   "902a07f881ebea3703badaa594ecbdf8fd1709211ba16769f73e76f348e2755d"
   "bba2f94c1869ef71e726f56f8ece987f345c622e8b5c2a5466d41093c0dc2982"
   "e6203d96f539b542347a08e87fc6e248a346d61a505f52add7f768a5203d70b8"
   "68b6ec92ef7a83a4e6d1e1d259018705755d812175489fae83c4ab2957f69a99"
   "9394ac7a243a5c1cd85f92b8648a8e0d23165fdd86fad06990bfd16fb3293379"}
};

const Attestation::PCRValue Attestation::kKnownPCRValues[] = {
  { false, false, kVerified  },
  { false, false, kDeveloper },
  { false, true,  kVerified  },
  { false, true,  kDeveloper },
  { true,  false, kVerified  },
  { true,  false, kDeveloper },
  { true,  true,  kVerified  },
  { true,  true,  kDeveloper }
};

bool Attestation::IsPreparedForEnrollment() {
  base::AutoLock lock(prepare_lock_);
  if (!is_prepared_ && tpm_) {
    EncryptedDatabase encrypted_db;
    if (!LoadDatabase(&encrypted_db)) {
      LOG(INFO) << "Attestation: Attestation data not found.";
      return false;
    }
    if (!DecryptDatabase(encrypted_db, &database_pb_)) {
      LOG(ERROR) << "Attestation: Attestation data invalid.";
      return false;
    }
    LOG(INFO) << "Attestation: Valid attestation data exists.";
    // Make sure the owner password is not being held on our account.
    tpm_->RemoveOwnerDependency(Tpm::kAttestation);
    is_prepared_ = true;
  }
  return is_prepared_;
}

void Attestation::PrepareForEnrollment() {
  // If there is no TPM, we have no work to do.
  if (!tpm_)
    return;
  if (IsPreparedForEnrollment())
    return;
  base::TimeTicks start = base::TimeTicks::Now();
  LOG(INFO) << "Attestation: Preparing for enrollment...";
  SecureBlob ek_public_key;
  if (!tpm_->GetEndorsementPublicKey(&ek_public_key)) {
    LOG(ERROR) << "Attestation: Failed to get EK public key.";
    return;
  }
  // Create an AIK.
  SecureBlob identity_public_key_der;
  SecureBlob identity_public_key;
  SecureBlob identity_key_blob;
  SecureBlob identity_binding;
  SecureBlob identity_label;
  SecureBlob pca_public_key;
  SecureBlob endorsement_credential;
  SecureBlob platform_credential;
  SecureBlob conformance_credential;
  if (!tpm_->MakeIdentity(&identity_public_key_der, &identity_public_key,
                          &identity_key_blob, &identity_binding,
                          &identity_label, &pca_public_key,
                          &endorsement_credential, &platform_credential,
                          &conformance_credential)) {
    LOG(ERROR) << "Attestation: Failed to make AIK.";
    return;
  }

  // Quote PCR0.
  SecureBlob external_data;
  if (!tpm_->GetRandomData(kQuoteExternalDataSize, &external_data)) {
    LOG(ERROR) << "Attestation: GetRandomData failed.";
    return;
  }
  SecureBlob quoted_pcr_value;
  SecureBlob quoted_data;
  SecureBlob quote;
  if (!tpm_->QuotePCR0(identity_key_blob, external_data, &quoted_pcr_value,
                       &quoted_data, &quote)) {
    LOG(ERROR) << "Attestation: Failed to generate quote.";
    return;
  }

  // Create a delegate so we can activate the AIK later.
  SecureBlob delegate_blob;
  SecureBlob delegate_secret;
  if (!tpm_->CreateDelegate(identity_key_blob, &delegate_blob,
                            &delegate_secret)) {
    LOG(ERROR) << "Attestation: Failed to create delegate.";
    return;
  }

  // Assemble a protobuf to store locally.
  base::AutoLock lock(prepare_lock_);
  TPMCredentials* credentials_pb = database_pb_.mutable_credentials();
  credentials_pb->set_endorsement_public_key(ek_public_key.data(),
                                             ek_public_key.size());
  credentials_pb->set_endorsement_credential(endorsement_credential.data(),
                                             endorsement_credential.size());
  credentials_pb->set_platform_credential(platform_credential.data(),
                                          platform_credential.size());
  credentials_pb->set_conformance_credential(conformance_credential.data(),
                                             conformance_credential.size());
  IdentityKey* key_pb = database_pb_.mutable_identity_key();
  key_pb->set_identity_public_key(identity_public_key_der.data(),
                                  identity_public_key_der.size());
  key_pb->set_identity_key_blob(identity_key_blob.data(),
                                identity_key_blob.size());
  IdentityBinding* binding_pb = database_pb_.mutable_identity_binding();
  binding_pb->set_identity_binding(identity_binding.data(),
                                   identity_binding.size());
  binding_pb->set_identity_public_key_der(identity_public_key_der.data(),
                                          identity_public_key_der.size());
  binding_pb->set_identity_public_key(identity_public_key.data(),
                                      identity_public_key.size());
  binding_pb->set_identity_label(identity_label.data(), identity_label.size());
  binding_pb->set_pca_public_key(pca_public_key.data(), pca_public_key.size());
  Quote* quote_pb = database_pb_.mutable_pcr0_quote();
  quote_pb->set_quote(quote.data(), quote.size());
  quote_pb->set_quoted_data(quoted_data.data(), quoted_data.size());
  quote_pb->set_quoted_pcr_value(quoted_pcr_value.data(),
                                 quoted_pcr_value.size());
  Delegation* delegate_pb = database_pb_.mutable_delegate();
  delegate_pb->set_blob(delegate_blob.data(), delegate_blob.size());
  delegate_pb->set_secret(delegate_secret.data(), delegate_secret.size());

  if (!tpm_->GetRandomData(kCipherKeySize, &database_key_)) {
    LOG(ERROR) << "Attestation: GetRandomData failed.";
    return;
  }
  SecureBlob sealed_key;
  if (!tpm_->SealToPCR0(database_key_, &sealed_key)) {
    LOG(ERROR) << "Attestation: Failed to seal cipher key.";
    return;
  }
  EncryptedDatabase encrypted_pb;
  encrypted_pb.set_sealed_key(sealed_key.data(), sealed_key.size());
  if (!EncryptDatabase(database_pb_, &encrypted_pb)) {
    LOG(ERROR) << "Attestation: Failed to encrypt db.";
    return;
  }
  if (!StoreDatabase(encrypted_pb)) {
    LOG(ERROR) << "Attestation: Failed to store db.";
    return;
  }
  tpm_->RemoveOwnerDependency(Tpm::kAttestation);
  base::TimeDelta delta = (base::TimeTicks::Now() - start);
  LOG(INFO) << "Attestation: Prepared successfully (" << delta.InMilliseconds()
            << "ms).";
}

bool Attestation::Verify() {
  if (!tpm_)
    return false;
  LOG(INFO) << "Attestation: Verifying data.";
  EncryptedDatabase encrypted_db;
  if (!LoadDatabase(&encrypted_db)) {
    LOG(INFO) << "Attestation: Attestation data not found.";
    return false;
  }
  if (!DecryptDatabase(encrypted_db, &database_pb_)) {
    LOG(ERROR) << "Attestation: Attestation data invalid.";
    return false;
  }
  const TPMCredentials& credentials = database_pb_.credentials();
  SecureBlob ek_public_key = ConvertStringToBlob(
      credentials.endorsement_public_key());
  if (!VerifyEndorsementCredential(
          ConvertStringToBlob(credentials.endorsement_credential()),
          ek_public_key)) {
    LOG(ERROR) << "Attestation: Bad endorsement credential.";
    return false;
  }
  if (!VerifyIdentityBinding(database_pb_.identity_binding())) {
    LOG(ERROR) << "Attestation: Bad identity binding.";
    return false;
  }
  SecureBlob aik_public_key = ConvertStringToBlob(
      database_pb_.identity_binding().identity_public_key_der());
  if (!VerifyQuote(aik_public_key, database_pb_.pcr0_quote())) {
    LOG(ERROR) << "Attestation: Bad PCR0 quote.";
    return false;
  }
  SecureBlob nonce;
  if (!tpm_->GetRandomData(kNonceSize, &nonce)) {
    LOG(ERROR) << "Attestation: GetRandomData failed.";
    return false;
  }
  SecureBlob identity_key_blob = ConvertStringToBlob(
      database_pb_.identity_key().identity_key_blob());
  SecureBlob public_key;
  SecureBlob key_blob;
  SecureBlob key_info;
  SecureBlob proof;
  if (!tpm_->CreateCertifiedKey(identity_key_blob, nonce, &public_key,
                                &key_blob, &key_info, &proof)) {
    LOG(ERROR) << "Attestation: Failed to create certified key.";
    return false;
  }
  if (!VerifyCertifiedKey(aik_public_key, public_key, key_info, proof)) {
    LOG(ERROR) << "Attestation: Bad certified key.";
    return false;
  }
  // TODO(dkrahn): Enable this check. It is currently disabled until we can get
  // support for owner delegation in trousers (crosbug.com/33597).
  //  SecureBlob delegate_blob =
  //      ConvertStringToBlob(database_pb_.delegate().blob());
  //  SecureBlob delegate_secret =
  //      ConvertStringToBlob(database_pb_.delegate().secret());
  //  SecureBlob aik_public_key_tpm = ConvertStringToBlob(
  //      database_pb_.identity_binding().identity_public_key());
  //  if (!VerifyActivateIdentity(delegate_blob, delegate_secret,
  //                              identity_key_blob, aik_public_key_tpm,
  //                              ek_public_key)) {
  //    LOG(ERROR) << "Attestation: Failed to verify owner delegation.";
  //    return false;
  //  }
  LOG(INFO) << "Attestation: Verified OK.";
  return true;
}

SecureBlob Attestation::ConvertStringToBlob(const string& s) {
  return SecureBlob(s.data(), s.length());
}

string Attestation::ConvertBlobToString(const chromeos::Blob& blob) {
  return string(reinterpret_cast<const char*>(&blob.front()), blob.size());
}

SecureBlob Attestation::SecureCat(const SecureBlob& blob1,
                                  const SecureBlob& blob2) {
  SecureBlob result(blob1.size() + blob2.size());
  unsigned char* buffer = const_cast<unsigned char*>(&result.front());
  memcpy(buffer, blob1.const_data(), blob1.size());
  memcpy(buffer + blob1.size(), blob2.const_data(), blob2.size());
  return SecureBlob(result.begin(), result.end());
}

bool Attestation::EncryptDatabase(const AttestationDatabase& db,
                                  EncryptedDatabase* encrypted_db) {
  SecureBlob iv;
  if (!tpm_->GetRandomData(kCipherBlockSize, &iv)) {
    LOG(ERROR) << "GetRandomData failed.";
    return false;
  }
  string serial_string;
  if (!db.SerializeToString(&serial_string)) {
    LOG(ERROR) << "Failed to serialize db.";
    return false;
  }
  SecureBlob serial_data(serial_string.data(), serial_string.size());
  SecureBlob encrypted_data;
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

bool Attestation::DecryptDatabase(const EncryptedDatabase& encrypted_db,
                                  AttestationDatabase* db) {
  SecureBlob sealed_key(encrypted_db.sealed_key().data(),
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
  SecureBlob iv(encrypted_db.iv().data(), encrypted_db.iv().length());
  SecureBlob encrypted_data(encrypted_db.encrypted_data().data(),
                            encrypted_db.encrypted_data().length());
  SecureBlob serial_db;
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

string Attestation::ComputeHMAC(const EncryptedDatabase& encrypted_db) {
  SecureBlob hmac_input = SecureCat(
      ConvertStringToBlob(encrypted_db.iv()),
      ConvertStringToBlob(encrypted_db.encrypted_data()));
  return ConvertBlobToString(CryptoLib::HmacSha512(database_key_, hmac_input));
}

bool Attestation::StoreDatabase(const EncryptedDatabase& encrypted_db) {
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

bool Attestation::LoadDatabase(EncryptedDatabase* encrypted_db) {
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

bool Attestation::VerifyEndorsementCredential(const SecureBlob& credential,
                                              const SecureBlob& public_key) {
  const unsigned char* asn1_ptr = &credential.front();
  X509* x509 = d2i_X509(NULL, &asn1_ptr, credential.size());
  if (!x509) {
    LOG(ERROR) << "Failed to parse endorsement credential.";
    return false;
  }
  // Manually verify the certificate signature.
  char issuer[100];  // A longer CN will truncate.
  X509_NAME_get_text_by_NID(x509->cert_info->issuer, NID_commonName, issuer,
                            arraysize(issuer));
  EVP_PKEY* issuer_key = GetAuthorityPublicKey(issuer);
  if (!issuer_key) {
    LOG(ERROR) << "Unknown endorsement credential issuer.";
    return false;
  }
  if (!X509_verify(x509, issuer_key)) {
    LOG(ERROR) << "Bad endorsement credential signature.";
    EVP_PKEY_free(issuer_key);
    return false;
  }
  EVP_PKEY_free(issuer_key);
  // Verify that the given public key matches the public key in the credential.
  // Note: Do not use any openssl functions that attempt to decode the public
  // key. These will fail because openssl does not recognize the OAEP key type.
  SecureBlob credential_public_key(x509->cert_info->key->public_key->data,
                                   x509->cert_info->key->public_key->length);
  if (credential_public_key.size() != public_key.size() ||
      memcmp(&credential_public_key.front(),
             &public_key.front(),
             public_key.size()) != 0) {
    LOG(ERROR) << "Bad endorsement credential public key.";
    return false;
  }
  X509_free(x509);
  return true;
}

bool Attestation::VerifyIdentityBinding(const IdentityBinding& binding) {
  // Reconstruct and hash a serialized TPM_IDENTITY_CONTENTS structure.
  const unsigned char header[] = {1, 1, 0, 0, 0, 0, 0, 0x79};
  string label_ca = binding.identity_label() + binding.pca_public_key();
  SecureBlob label_ca_digest = CryptoLib::Sha1(ConvertStringToBlob(label_ca));
  ClearString(&label_ca);
  // The signed data is header + digest + pubkey.
  SecureBlob contents = SecureCat(SecureCat(
      SecureBlob(header, arraysize(header)),
      label_ca_digest),
      ConvertStringToBlob(binding.identity_public_key()));
  // Now verify the signature.
  if (!VerifySignature(ConvertStringToBlob(binding.identity_public_key_der()),
                       contents,
                       ConvertStringToBlob(binding.identity_binding()))) {
    LOG(ERROR) << "Failed to verify identity binding signature.";
    return false;
  }
  return true;
}

bool Attestation::VerifyQuote(const SecureBlob& aik_public_key,
                              const Quote& quote) {
  if (!VerifySignature(aik_public_key,
                       ConvertStringToBlob(quote.quoted_data()),
                       ConvertStringToBlob(quote.quote()))) {
    LOG(ERROR) << "Failed to verify quote signature.";
    return false;
  }

  // Check that the quoted value matches the given PCR value. We can verify this
  // by reconstructing the TPM_PCR_COMPOSITE structure the TPM would create.
  const uint8_t header[] = {
    static_cast<uint8_t>(0), static_cast<uint8_t>(2),
    static_cast<uint8_t>(1), static_cast<uint8_t>(0),
    static_cast<uint8_t>(0), static_cast<uint8_t>(0),
    static_cast<uint8_t>(0),
    static_cast<uint8_t>(quote.quoted_pcr_value().size())};
  SecureBlob pcr_composite = SecureCat(
      SecureBlob(header, arraysize(header)),
      ConvertStringToBlob(quote.quoted_pcr_value()));
  SecureBlob pcr_digest = CryptoLib::Sha1(pcr_composite);
  SecureBlob quoted_data = ConvertStringToBlob(quote.quoted_data());
  if (search(quoted_data.begin(), quoted_data.end(),
             pcr_digest.begin(), pcr_digest.end()) == quoted_data.end()) {
    LOG(ERROR) << "PCR0 value mismatch.";
    return false;
  }

  // Check if the PCR0 value represents a known mode.
  for (size_t i = 0; i < arraysize(kKnownPCRValues); ++i) {
    SecureBlob settings_blob(3);
    settings_blob[0] = kKnownPCRValues[i].developer_mode_enabled;
    settings_blob[1] = kKnownPCRValues[i].recovery_mode_enabled;
    settings_blob[2] = kKnownPCRValues[i].firmware_type;
    SecureBlob settings_digest = CryptoLib::Sha1(settings_blob);
    chromeos::Blob extend_pcr_value(kDigestSize, 0);
    extend_pcr_value.insert(extend_pcr_value.end(), settings_digest.begin(),
                            settings_digest.end());
    SecureBlob final_pcr_value = CryptoLib::Sha1(extend_pcr_value);
    if (quote.quoted_pcr_value().size() == final_pcr_value.size() &&
        0 == memcmp(quote.quoted_pcr_value().data(), final_pcr_value.data(),
                    final_pcr_value.size())) {
      string description = "Developer Mode: ";
      description += kKnownPCRValues[i].developer_mode_enabled ? "On" : "Off";
      description += ", Recovery Mode: ";
      description += kKnownPCRValues[i].recovery_mode_enabled ? "On" : "Off";
      description += ", Firmware Type: ";
      description += (kKnownPCRValues[i].firmware_type == 1) ? "Verified" :
                     "Developer";
      LOG(INFO) << "PCR0: " << description;
      return true;
    }
  }
  LOG(WARNING) << "PCR0 value not recognized.";
  return true;
}

bool Attestation::VerifyCertifiedKey(
    const SecureBlob& aik_public_key,
    const SecureBlob& certified_public_key,
    const SecureBlob& certified_key_info,
    const SecureBlob& proof) {
  string key_info = ConvertBlobToString(certified_key_info);
  if (!VerifySignature(aik_public_key, certified_key_info, proof)) {
    LOG(ERROR) << "Failed to verify certified key proof signature.";
    return false;
  }
  const unsigned char* asn1_ptr = &certified_public_key.front();
  RSA* rsa = d2i_RSAPublicKey(NULL, &asn1_ptr, certified_public_key.size());
  if (!rsa) {
    LOG(ERROR) << "Failed to decode certified public key.";
    return false;
  }
  SecureBlob modulus(BN_num_bytes(rsa->n));
  BN_bn2bin(rsa->n, const_cast<unsigned char*>(&modulus.front()));
  RSA_free(rsa);
  SecureBlob key_digest = CryptoLib::Sha1(modulus);
  if (std::search(certified_key_info.begin(),
                  certified_key_info.end(),
                  key_digest.begin(),
                  key_digest.end()) == certified_key_info.end()) {
    LOG(ERROR) << "Cerified public key mismatch.";
    return false;
  }
  return true;
}

EVP_PKEY* Attestation::GetAuthorityPublicKey(const char* issuer_name) {
  const int kNumIssuers = arraysize(kKnownEndorsementCA);
  for (int i = 0; i < kNumIssuers; ++i) {
    if (0 == strcmp(issuer_name, kKnownEndorsementCA[i].issuer)) {
      RSA* rsa = RSA_new();
      if (!rsa)
        return NULL;
      rsa->e = BN_new();
      if (!rsa->e) {
        RSA_free(rsa);
        return NULL;
      }
      BN_set_word(rsa->e, kWellKnownExponent);
      rsa->n = BN_new();
      if (!rsa->n) {
        RSA_free(rsa);
        return NULL;
      }
      BN_hex2bn(&rsa->n, kKnownEndorsementCA[i].modulus);
      EVP_PKEY* pkey = EVP_PKEY_new();
      if (!pkey) {
        RSA_free(rsa);
        return NULL;
      }
      EVP_PKEY_assign_RSA(pkey, rsa);
      return pkey;
    }
  }
  return NULL;
}

bool Attestation::VerifySignature(const SecureBlob& public_key,
                                  const SecureBlob& signed_data,
                                  const SecureBlob& signature) {
  const unsigned char* asn1_ptr = &public_key.front();
  RSA* rsa = d2i_RSAPublicKey(NULL, &asn1_ptr, public_key.size());
  if (!rsa) {
    LOG(ERROR) << "Failed to decode public key.";
    return false;
  }
  SecureBlob digest = CryptoLib::Sha1(signed_data);
  if (!RSA_verify(NID_sha1, &digest.front(), digest.size(),
                  const_cast<unsigned char*>(&signature.front()),
                  signature.size(), rsa)) {
    LOG(ERROR) << "Failed to verify signature.";
    RSA_free(rsa);
    return false;
  }
  RSA_free(rsa);
  return true;
}

void Attestation::ClearDatabase() {
  if (database_pb_.has_credentials()) {
    TPMCredentials* credentials = database_pb_.mutable_credentials();
    ClearString(credentials->mutable_endorsement_public_key());
    ClearString(credentials->mutable_endorsement_credential());
    ClearString(credentials->mutable_platform_credential());
    ClearString(credentials->mutable_conformance_credential());
  }
  if (database_pb_.has_identity_binding()) {
    IdentityBinding* binding = database_pb_.mutable_identity_binding();
    ClearString(binding->mutable_identity_binding());
    ClearString(binding->mutable_identity_public_key_der());
    ClearString(binding->mutable_identity_public_key());
    ClearString(binding->mutable_identity_label());
    ClearString(binding->mutable_pca_public_key());
  }
  if (database_pb_.has_identity_key()) {
    IdentityKey* key = database_pb_.mutable_identity_key();
    ClearString(key->mutable_identity_public_key());
    ClearString(key->mutable_identity_key_blob());
    ClearString(key->mutable_identity_credential());
  }
  if (database_pb_.has_pcr0_quote()) {
    Quote* quote = database_pb_.mutable_pcr0_quote();
    ClearString(quote->mutable_quote());
    ClearString(quote->mutable_quoted_data());
    ClearString(quote->mutable_quoted_pcr_value());
  }
  if (database_pb_.has_delegate()) {
    Delegation* delegate = database_pb_.mutable_delegate();
    ClearString(delegate->mutable_blob());
    ClearString(delegate->mutable_secret());
  }
}

void Attestation::ClearString(string* s) {
  chromeos::SecureMemset(const_cast<char*>(s->data()), 0, s->length());
}

bool Attestation::VerifyActivateIdentity(const SecureBlob& delegate_blob,
                                         const SecureBlob& delegate_secret,
                                         const SecureBlob& identity_key_blob,
                                         const SecureBlob& identity_public_key,
                                         const SecureBlob& ek_public_key) {
  const char* kTestCredential = "test";
  const uint8_t kAlgAES256 = 9;  // This comes from TPM_ALG_AES256.
  const uint8_t kEncModeCBC = 2;  // This comes from TPM_SYM_MODE_CBC.
  const uint8_t kAsymContentHeader[] =
      {0, 0, 0, kAlgAES256, 0, kEncModeCBC, 0, kCipherKeySize};
  const uint8_t kSymContentHeader[12] = {0};

  // Generate an AES key and encrypt the credential.
  SecureBlob aes_key(kCipherKeySize);
  CryptoLib::GetSecureRandom(const_cast<unsigned char*>(&aes_key.front()),
                             aes_key.size());
  SecureBlob credential(kTestCredential, strlen(kTestCredential));
  SecureBlob encrypted_credential;
  if (!tpm_->TssCompatibleEncrypt(aes_key, credential, &encrypted_credential)) {
    LOG(ERROR) << "Failed to encrypt credential.";
    return false;
  }

  // Construct a TPM_ASYM_CA_CONTENTS structure.
  SecureBlob public_key_digest = CryptoLib::Sha1(identity_public_key);
  SecureBlob asym_content = SecureCat(SecureCat(
      SecureBlob(kAsymContentHeader, arraysize(kAsymContentHeader)),
      aes_key),
      public_key_digest);

  // Encrypt the TPM_ASYM_CA_CONTENTS with the EK public key.
  const unsigned char* asn1_ptr = &ek_public_key[0];
  RSA* rsa = d2i_RSAPublicKey(NULL, &asn1_ptr, ek_public_key.size());
  if (!rsa) {
    LOG(ERROR) << "Failed to decode EK public key.";
    return false;
  }
  SecureBlob encrypted_asym_content;
  if (!tpm_->TpmCompatibleOAEPEncrypt(rsa, asym_content,
                                      &encrypted_asym_content)) {
    LOG(ERROR) << "Failed to encrypt with EK public key.";
    return false;
  }

  // Construct a TPM_SYM_CA_ATTESTATION structure.
  uint32_t length = htonl(encrypted_credential.size());
  SecureBlob length_blob(sizeof(uint32_t));
  memcpy(length_blob.data(), &length, sizeof(uint32_t));
  SecureBlob sym_content = SecureCat(SecureCat(
      length_blob,
      SecureBlob(kSymContentHeader, arraysize(kSymContentHeader))),
      encrypted_credential);

  // Attempt to activate the identity.
  SecureBlob credential_out;
  if (!tpm_->ActivateIdentity(delegate_blob, delegate_secret, identity_key_blob,
                              encrypted_asym_content, sym_content,
                              &credential_out)) {
    LOG(ERROR) << "Failed to activate identity.";
    return false;
  }
  if (credential.size() != credential_out.size() ||
      chromeos::SafeMemcmp(credential.data(), credential_out.data(),
                           credential.size()) != 0) {
    LOG(ERROR) << "Invalid identity credential.";
    return false;
  }
  return true;
}

}

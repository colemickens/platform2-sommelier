// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/boot_lockbox.h"

#include <string>

#include <base/stl_util.h>
#include <openssl/objects.h>
#include <openssl/rsa.h>

#include "cryptohome/crypto.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/platform.h"
#include "cryptohome/tpm.h"

using chromeos::SecureBlob;

namespace {

// The DER encoding of SHA-256 DigestInfo as defined in PKCS #1.
const unsigned char kSha256DigestInfo[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03,
    0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20
};

const int kPCRIndex = 15;

const unsigned char kPCRValue[20] = {0};

// This is an arbitrary value, our only goal is for the PCR to be non-zero.
const char kPCRExtension[] = "CROS_PCR15_845A4A757B94";

const char kKeyFilePath[] = "/var/lib/boot-lockbox/key";

// So we can use scoped_ptr with openssl types.
struct RSADeleter {
  void operator()(void* ptr) const {
    if (ptr)
      RSA_free(reinterpret_cast<RSA*>(ptr));
  }
};

}  // namespace

namespace cryptohome {

BootLockbox::BootLockbox(Tpm* tpm, Platform* platform, Crypto* crypto)
    : tpm_(tpm), platform_(platform), crypto_(crypto) {
}

BootLockbox::~BootLockbox() {}

bool BootLockbox::Sign(const chromeos::SecureBlob& data,
                       chromeos::SecureBlob* signature) {
  CHECK(signature);
  chromeos::SecureBlob key_blob;
  if (!GetKeyBlob(&key_blob)) {
    return false;
  }
  chromeos::SecureBlob der_header(kSha256DigestInfo,
                                  arraysize(kSha256DigestInfo));
  chromeos::SecureBlob der_encoded_input = SecureBlob::Combine(
      der_header,
      CryptoLib::Sha256(data));
  return tpm_->Sign(key_blob, der_encoded_input, signature);
}

bool BootLockbox::Verify(const chromeos::SecureBlob& data,
                         const chromeos::SecureBlob& signature) {
  chromeos::SecureBlob public_key;
  if (!GetPublicKey(&public_key)) {
    return false;
  }
  if (!VerifySignature(public_key, data, signature)) {
    return false;
  }
  chromeos::SecureBlob key_blob;
  if (!GetKeyBlob(&key_blob)) {
    return false;
  }
  chromeos::SecureBlob pcr_value(kPCRValue, arraysize(kPCRValue));
  if (!tpm_->VerifyPCRBoundKey(kPCRIndex, pcr_value, key_blob)) {
    return false;
  }
  return true;
}

bool BootLockbox::FinalizeBoot() {
  if (IsFinalized()) {
      // The PCR is already not at the initial value, no need to extend again.
      return true;
  }
  return tpm_->ExtendPCR(kPCRIndex,
                         chromeos::SecureBlob(kPCRExtension,
                                              arraysize(kPCRExtension)));
}

bool BootLockbox::IsFinalized() {
  chromeos::SecureBlob actual_pcr_value;
  return tpm_->ReadPCR(kPCRIndex, &actual_pcr_value) &&
         actual_pcr_value.size() == arraysize(kPCRValue) &&
         memcmp(actual_pcr_value.data(), kPCRValue, arraysize(kPCRValue));
}

bool BootLockbox::GetKeyBlob(chromeos::SecureBlob* key_blob) {
  if (!key_.has_key_blob() && !LoadKey(&key_) && !CreateKey(&key_)) {
    return false;
  }
  CHECK(key_.has_key_blob());
  if (key_blob) {
    chromeos::SecureBlob tmp = SecureBlob(key_.key_blob());
    key_blob->swap(tmp);
  }
  return true;
}

bool BootLockbox::GetPublicKey(chromeos::SecureBlob* public_key) {
  if (!key_.has_public_key_der() && !LoadKey(&key_)) {
    return false;
  }
  CHECK(key_.has_public_key_der());
  if (public_key) {
    chromeos::SecureBlob tmp = SecureBlob(key_.public_key_der());
    public_key->swap(tmp);
  }
  return true;
}

bool BootLockbox::LoadKey(BootLockboxKey* key) {
  std::string file_contents;
  if (!platform_->ReadFileToString(kKeyFilePath, &file_contents)) {
    return false;
  }
  chromeos::SecureBlob protobuf;
  if (!crypto_->DecryptWithTpm(file_contents, &protobuf)) {
    LOG(WARNING) << "Failed to decrypt boot-lockbox key.";
    return false;
  }
  if (!key->ParseFromArray(protobuf.const_data(), protobuf.size())) {
    LOG(ERROR) << "Invalid boot-lockbox key.";
    return false;
  }
  return true;
}

bool BootLockbox::SaveKey(const BootLockboxKey& key) {
  chromeos::SecureBlob protobuf(key.ByteSize());
  if (!key.SerializeToArray(protobuf.data(), protobuf.size())) {
    LOG(ERROR) << "Failed to serialize boot-lockbox key.";
    return false;
  }
  std::string encrypted_protobuf;
  if (!crypto_->EncryptWithTpm(protobuf, &encrypted_protobuf)) {
    LOG(ERROR) << "Failed to encrypt boot-lockbox key.";
    return false;
  }
  if (!platform_->WriteStringToFile(kKeyFilePath, encrypted_protobuf)) {
    LOG(ERROR) << "Failed to write boot-lockbox key.";
    return false;
  }
  return true;
}

bool BootLockbox::CreateKey(BootLockboxKey* key) {
  LOG(INFO) <<  "Creating new boot-lockbox key.";
  chromeos::SecureBlob key_blob;
  chromeos::SecureBlob public_key;
  chromeos::SecureBlob pcr_value(kPCRValue, arraysize(kPCRValue));
  if (!tpm_->CreatePCRBoundKey(kPCRIndex, pcr_value, &key_blob, &public_key)) {
    LOG(ERROR) << "Failed to create boot-lockbox key.";
    return false;
  }
  key->set_key_blob(key_blob.to_string());
  key->set_public_key_der(public_key.to_string());
  return SaveKey(*key);
}

bool BootLockbox::VerifySignature(const chromeos::SecureBlob& public_key,
                                  const chromeos::SecureBlob& signed_data,
                                  const chromeos::SecureBlob& signature) {
  const unsigned char* asn1_ptr = &public_key.front();
  scoped_ptr<RSA, RSADeleter> rsa(
      d2i_RSAPublicKey(NULL, &asn1_ptr, public_key.size()));
  if (!rsa.get()) {
    LOG(ERROR) << "Failed to decode public key.";
    return false;
  }
  chromeos::SecureBlob digest = CryptoLib::Sha256(signed_data);
  if (!RSA_verify(NID_sha256, &digest.front(), digest.size(),
                  const_cast<unsigned char*>(&signature.front()),
                  signature.size(), rsa.get())) {
    LOG(ERROR) << "Failed to verify signature.";
    return false;
  }
  return true;
}

}  // namespace cryptohome

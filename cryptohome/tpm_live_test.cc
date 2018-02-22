// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test methods that run on a real TPM

#include "cryptohome/tpm_live_test.h"

#include <crypto/scoped_openssl_types.h>
#include <crypto/sha2.h>
#include <openssl/rsa.h>

#include "cryptohome/cryptolib.h"

using brillo::SecureBlob;

namespace cryptohome {

TpmLiveTest::TpmLiveTest() : tpm_(Tpm::GetSingleton()) {}

bool TpmLiveTest::RunLiveTests(SecureBlob owner_password) {
  // First we run tests that do not need the owner_password
  if (!PCRKeyTest()) {
    LOG(ERROR) << "Error running PCRKeyTest.";
    return false;
  }
  if (!DecryptionKeyTest()) {
    LOG(ERROR) << "Error running Decryption test.";
    return false;
  }

  if (!owner_password.empty()) {
    VLOG(1) << "Running tests that require owner password.";
    tpm_->SetOwnerPassword(owner_password);
    if (!NvramTest()) {
      LOG(ERROR) << "Error running NvramTest.";
      return false;
    }
  }
  LOG(INFO) << "All tests run successfully.";
  return true;
}

bool TpmLiveTest::PCRKeyTest() {
  int index = 5;
  SecureBlob pcr_data;
  if (!tpm_->ReadPCR(index, &pcr_data)) {
    LOG(ERROR) << "Error reading pcr value from TPM.";
    return false;
  }
  SecureBlob pcr_bound_key;
  SecureBlob public_key_der;
  SecureBlob creation_blob;
  if (!tpm_->CreatePCRBoundKey(index, SecureBlob(pcr_data),
                               &pcr_bound_key, &public_key_der,
                               &creation_blob)) {
    LOG(ERROR) << "Error creating PCR bound key.";
    return false;
  }
  SecureBlob input_data("input_data");
  SecureBlob signature;
  if (!tpm_->Sign(pcr_bound_key, input_data, index, &signature)) {
    LOG(ERROR) << "Error signing with PCR bound key.";
    return false;
  }
  const unsigned char* public_key_data = public_key_der.data();
  crypto::ScopedRSA rsa(
      d2i_RSAPublicKey(nullptr, &public_key_data, public_key_der.size()));
  if (!rsa.get()) {
    LOG(ERROR) << "Failed to decode public key.";
    return false;
  }
  SecureBlob digest = CryptoLib::Sha256(input_data);
  if (!RSA_verify(NID_sha256, digest.data(), digest.size(),
                  signature.data(), signature.size(), rsa.get())) {
    LOG(ERROR) << "Failed to verify signature.";
    return false;
  }
  if (!tpm_->VerifyPCRBoundKey(index, SecureBlob(pcr_data),
                               pcr_bound_key, creation_blob)) {
    LOG(ERROR) << "Error verifying PCR bound key.";
    return false;
  }
  if (!tpm_->ExtendPCR(index, SecureBlob("01234567890123456789"))) {
    LOG(ERROR) << "Error extending PCR.";
    return false;
  }
  if (tpm_->Sign(pcr_bound_key, input_data, index, &signature)) {
    LOG(ERROR) << "Sign succeeded without the correct PCR state.";
    return false;
  }
  VLOG(1) << "PCRKeyTest eneded successfully.";
  return true;
}

bool TpmLiveTest::DecryptionKeyTest() {
  SecureBlob n;
  SecureBlob p;
  uint32_t tpm_key_bits = 2048;
  if (!CryptoLib::CreateRsaKey(tpm_key_bits, &n, &p)) {
    LOG(ERROR) << "Error creating RSA key.";
    return false;
  }
  SecureBlob wrapped_key;
  if (!tpm_->WrapRsaKey(n, p, &wrapped_key)) {
    LOG(ERROR) << "Error wrapping RSA key.";
    return false;
  }
  ScopedKeyHandle handle;
  if (tpm_->LoadWrappedKey(wrapped_key, &handle) != Tpm::kTpmRetryNone) {
    LOG(ERROR) << "Error loading key.";
    return false;
  }
  SecureBlob aes_key(32, 'a');
  SecureBlob plaintext(32, 'b');
  SecureBlob ciphertext;
  if (tpm_->EncryptBlob(handle.value(), plaintext, aes_key, &ciphertext) !=
      Tpm::kTpmRetryNone) {
    LOG(ERROR) << "Error encrypting blob.";
    return false;
  }
  SecureBlob decrypted_plaintext;
  if (tpm_->DecryptBlob(handle.value(), ciphertext, aes_key,
                        &decrypted_plaintext) != Tpm::kTpmRetryNone) {
    LOG(ERROR) << "Error decrypting blob.";
    return false;
  }
  if (plaintext != decrypted_plaintext) {
    LOG(ERROR) << "Decrypted plaintext does not match plaintext.";
    return false;
  }
  return true;
}

bool TpmLiveTest::NvramTest() {
  uint32_t index = 12;
  SecureBlob nvram_data("nvram_data");
  if (tpm_->IsNvramDefined(index)) {
    LOG(ERROR) << "Nvram index is already defined.";
    return false;
  }
  if (!tpm_->DefineNvram(index, nvram_data.size(),
                         Tpm::kTpmNvramWriteDefine |
                         Tpm::kTpmNvramBindToPCR0)) {
    LOG(ERROR) << "Defining Nvram index.";
    return false;
  }
  if (!tpm_->IsNvramDefined(index)) {
    LOG(ERROR) << "Nvram index is not defined after creating.";
    return false;
  }
  if (tpm_->GetNvramSize(index) != nvram_data.size()) {
    LOG(ERROR) << "Nvram space is of incorrect size.";
    return false;
  }
  if (tpm_->IsNvramLocked(index)) {
    LOG(ERROR) << "Nvram should not be locked before writing.";
    return false;
  }
  if (!tpm_->WriteNvram(index, nvram_data)) {
    LOG(ERROR) << "Error writing to Nvram.";
    return false;
  }
  if (!tpm_->WriteLockNvram(index)) {
    LOG(ERROR) << "Error locking Nvram space.";
    return false;
  }
  if (!tpm_->IsNvramLocked(index)) {
    LOG(ERROR) << "Nvram should be locked after locking.";
    return false;
  }
  SecureBlob data;
  if (!tpm_->ReadNvram(index, &data)) {
    LOG(ERROR) << "Error reading from Nvram.";
    return false;
  }
  if (data != nvram_data) {
    LOG(ERROR) << "Data read from Nvram did not match data written.";
    return false;
  }
  if (tpm_->WriteNvram(index, nvram_data)) {
    LOG(ERROR) << "We should not be able to write to a locked Nvram space.";
    return false;
  }
  if (!tpm_->DestroyNvram(index)) {
    LOG(ERROR) << "Error destroying Nvram space.";
    return false;
  }
  if (tpm_->IsNvramDefined(index)) {
    LOG(ERROR) << "Nvram still defined after it was destroyed.";
    return false;
  }
  VLOG(1) << "NvramTest ended successfully.";
  return true;
}

}  // namespace cryptohome

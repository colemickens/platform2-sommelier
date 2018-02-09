// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test methods that run on a real TPM

#include "cryptohome/tpm_live_test.h"

#include <map>
#include <memory>
#include <set>

#include <base/macros.h>
#include <base/memory/ptr_util.h>
#include <crypto/scoped_openssl_types.h>
#include <crypto/sha2.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "cryptohome/cryptolib.h"
#include "cryptohome/signature_sealing_backend.h"

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
  if (!SignatureSealedSecretTest()) {
    LOG(ERROR) << "Error running SignatureSealedSecretTest.";
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
  VLOG(1) << "PCRKeyTest started";
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
  VLOG(1) << "PCRKeyTest ended successfully.";
  return true;
}

bool TpmLiveTest::DecryptionKeyTest() {
  VLOG(1) << "DecryptionKeyTest started";
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
  VLOG(1) << "DecryptionKeyTest ended successfully.";
  return true;
}

bool TpmLiveTest::NvramTest() {
  VLOG(1) << "NvramTest started";
  uint32_t index = 12;
  SecureBlob nvram_data("nvram_data");
  if (tpm_->IsNvramDefined(index)) {
    if (!tpm_->DestroyNvram(index)) {
      LOG(ERROR) << "Error destroying old Nvram.";
      return false;
    }
    if (tpm_->IsNvramDefined(index)) {
      LOG(ERROR) << "Nvram still defined after it was destroyed.";
      return false;
    }
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

namespace {

class SignatureSealedSecretTestCase final {
 public:
  using Algorithm = SignatureSealingBackend::Algorithm;
  using UnsealingSession = SignatureSealingBackend::UnsealingSession;

  SignatureSealedSecretTestCase(
      Tpm* tpm,
      int key_size_bits,
      const std::string& test_case_description,
      const std::vector<Algorithm>& supported_algorithms,
      Algorithm expected_algorithm,
      int openssl_algorithm_nid)
      : tpm_(tpm),
        key_size_bits_(key_size_bits),
        test_case_description_(test_case_description),
        supported_algorithms_(supported_algorithms),
        expected_algorithm_(expected_algorithm),
        openssl_algorithm_nid_(openssl_algorithm_nid) {}

  SignatureSealedSecretTestCase(SignatureSealedSecretTestCase&& other) =
      default;

  void SetUp() { GenerateRsaKey(key_size_bits_, &pkey_, &key_spki_der_); }

  bool Run() {
    VLOG(1) << "SignatureSealedSecretTestCase: " << key_size_bits_
            << "-bit key, " << test_case_description_;
    // Create a secret.
    SignatureSealedData sealed_secret_data;
    if (!CreateSecret(&sealed_secret_data))
      return false;
    // Unseal the secret.
    SecureBlob first_challenge_value;
    SecureBlob first_challenge_signature;
    SecureBlob first_unsealed_value;
    if (!Unseal(sealed_secret_data, &first_challenge_value,
                &first_challenge_signature, &first_unsealed_value)) {
      return false;
    }
    // Unseal the secret again - the challenge is different, but the result is
    // the same.
    SecureBlob second_challenge_value;
    SecureBlob second_challenge_signature;
    SecureBlob second_unsealed_value;
    if (!Unseal(sealed_secret_data, &second_challenge_value,
                &second_challenge_signature, &second_unsealed_value)) {
      return false;
    }
    if (first_challenge_value == second_challenge_value) {
      LOG(ERROR) << "Error: challenge value collision";
      return false;
    }
    if (first_unsealed_value != second_unsealed_value) {
      LOG(ERROR) << "Error: unsealing result differs";
      return false;
    }
    // Unsealing with a bad challenge response fails.
    if (!CheckUnsealingFailsWithOldSignature(sealed_secret_data,
                                             first_challenge_signature) ||
        !CheckUnsealingFailsWithBadSignature(sealed_secret_data)) {
      return false;
    }
    // Unsealing with a bad key fails.
    if (!CheckUnsealingFailsWithWrongAlgorithm(sealed_secret_data) ||
        !CheckUnsealingFailsWithWrongKey(sealed_secret_data)) {
      return false;
    }
    // Unsealing after PCRs change fails.
    if (!CheckUnsealingFailsWithChangedPcrs(sealed_secret_data))
      return false;
    // Create and unseal another secret - it has a different value.
    SignatureSealedData another_sealed_secret_data;
    if (!CreateSecret(&another_sealed_secret_data))
      return false;
    SecureBlob third_challenge_value;
    SecureBlob third_challenge_signature;
    SecureBlob third_unsealed_value;
    if (!Unseal(another_sealed_secret_data, &third_challenge_value,
                &third_challenge_signature, &third_unsealed_value)) {
      return false;
    }
    if (first_unsealed_value == third_unsealed_value) {
      LOG(ERROR) << "Error: secret value collision";
      return false;
    }
    return true;
  }

 private:
  const std::vector<int> kPcrIndexes{0, 16};
  const std::set<int> kPcrIndexesSet{kPcrIndexes.begin(), kPcrIndexes.end()};
  static constexpr int kPcrIndexToExtend = 16;  // The Debug PCR.

  SignatureSealingBackend* backend() {
    return tpm_->GetSignatureSealingBackend();
  }

  static void GenerateRsaKey(int key_size_bits,
                             crypto::ScopedEVP_PKEY* pkey,
                             SecureBlob* key_spki_der) {
    crypto::ScopedEVP_PKEY_CTX pkey_context(
        EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
    CHECK(pkey_context);
    CHECK_GT(EVP_PKEY_keygen_init(pkey_context.get()), 0);
    CHECK_GT(
        EVP_PKEY_CTX_set_rsa_keygen_bits(pkey_context.get(), key_size_bits), 0);
    EVP_PKEY* pkey_raw = nullptr;
    CHECK_GT(EVP_PKEY_keygen(pkey_context.get(), &pkey_raw), 0);
    pkey->reset(pkey_raw);
    // Obtain the DER-encoded Subject Public Key Info.
    const int key_spki_der_length = i2d_PUBKEY(pkey->get(), nullptr);
    CHECK_GE(key_spki_der_length, 0);
    key_spki_der->resize(key_spki_der_length);
    unsigned char* key_spki_der_buffer = key_spki_der->data();
    CHECK_EQ(key_spki_der->size(),
             i2d_PUBKEY(pkey->get(), &key_spki_der_buffer));
  }

  bool CreateSecret(SignatureSealedData* sealed_secret_data) {
    std::map<int, SecureBlob> pcr_values;
    for (auto pcr_index : kPcrIndexes) {
      SecureBlob pcr_value;
      if (!tpm_->ReadPCR(pcr_index, &pcr_value)) {
        LOG(ERROR) << "Error reading PCR value";
        return false;
      }
      pcr_values[pcr_index] = pcr_value;
    }
    if (!backend()->CreateSealedSecret(key_spki_der_, supported_algorithms_,
                                       pcr_values, delegate_blob_,
                                       delegate_secret_, sealed_secret_data)) {
      LOG(ERROR) << "Error creating signature-sealed secret";
      return false;
    }
    if (!sealed_secret_data->has_tpm2_policy_signed_data()) {
      LOG(ERROR) << "Error: the resulting proto misses Tpm2PolicySignedData";
      return false;
    }
    return true;
  }

  bool Unseal(const SignatureSealedData& sealed_secret_data,
              SecureBlob* challenge_value,
              SecureBlob* challenge_signature,
              SecureBlob* unsealed_value) {
    std::unique_ptr<UnsealingSession> unsealing_session(
        backend()->CreateUnsealingSession(sealed_secret_data, key_spki_der_,
                                          supported_algorithms_, kPcrIndexesSet,
                                          delegate_blob_, delegate_secret_));
    if (!unsealing_session) {
      LOG(ERROR) << "Error starting the unsealing session";
      return false;
    }
    if (unsealing_session->GetChallengeAlgorithm() != expected_algorithm_) {
      LOG(ERROR) << "Wrong challenge signature algorithm";
      return false;
    }
    *challenge_value = unsealing_session->GetChallengeValue();
    if (challenge_value->empty()) {
      LOG(ERROR) << "The challenge is empty";
      return false;
    }
    if (!SignWithKey(*challenge_value, openssl_algorithm_nid_,
                     challenge_signature)) {
      LOG(ERROR) << "Error generating signature of challenge";
      return false;
    }
    if (!unsealing_session->Unseal(*challenge_signature, unsealed_value)) {
      LOG(ERROR) << "Error unsealing the secret";
      return false;
    }
    if (unsealed_value->empty()) {
      LOG(ERROR) << "Error: empty unsealing result";
      return false;
    }
    return true;
  }

  bool CheckUnsealingFailsWithOldSignature(
      const SignatureSealedData& sealed_secret_data,
      const SecureBlob& challenge_signature) {
    std::unique_ptr<UnsealingSession> unsealing_session(
        backend()->CreateUnsealingSession(sealed_secret_data, key_spki_der_,
                                          supported_algorithms_, kPcrIndexesSet,
                                          delegate_blob_, delegate_secret_));
    if (!unsealing_session) {
      LOG(ERROR) << "Error starting the unsealing session";
      return false;
    }
    SecureBlob unsealed_value;
    if (unsealing_session->Unseal(challenge_signature, &unsealed_value)) {
      LOG(ERROR) << "Error: unsealed completed with an old challenge signature";
      return false;
    }
    return true;
  }

  bool CheckUnsealingFailsWithBadSignature(
      const SignatureSealedData& sealed_secret_data) {
    std::unique_ptr<UnsealingSession> unsealing_session(
        backend()->CreateUnsealingSession(sealed_secret_data, key_spki_der_,
                                          supported_algorithms_, kPcrIndexesSet,
                                          delegate_blob_, delegate_secret_));
    if (!unsealing_session) {
      LOG(ERROR) << "Error starting the unsealing session";
      return false;
    }
    const int wrong_openssl_algorithm_nid =
        openssl_algorithm_nid_ == NID_sha1 ? NID_sha256 : NID_sha1;
    SecureBlob challenge_signature;
    if (!SignWithKey(unsealing_session->GetChallengeValue(),
                     wrong_openssl_algorithm_nid, &challenge_signature)) {
      LOG(ERROR) << "Error generating signature of challenge";
      return false;
    }
    SecureBlob unsealed_value;
    if (unsealing_session->Unseal(challenge_signature, &unsealed_value)) {
      LOG(ERROR) << "Error: unsealing completed with a wrong signature";
      return false;
    }
    return true;
  }

  bool CheckUnsealingFailsWithWrongAlgorithm(
      const SignatureSealedData& sealed_secret_data) {
    const Algorithm wrong_algorithm =
        expected_algorithm_ == Algorithm::kRsassaPkcs1V15Sha1
            ? Algorithm::kRsassaPkcs1V15Sha256
            : Algorithm::kRsassaPkcs1V15Sha1;
    if (backend()->CreateUnsealingSession(sealed_secret_data, key_spki_der_,
                                          {wrong_algorithm}, kPcrIndexesSet,
                                          delegate_blob_, delegate_secret_)) {
      LOG(ERROR) << "Error: unsealing session creation completed with a "
                    "wrong algorithm";
      return false;
    }
    return true;
  }

  bool CheckUnsealingFailsWithWrongKey(
      const SignatureSealedData& sealed_secret_data) {
    crypto::ScopedEVP_PKEY other_pkey;
    SecureBlob other_key_spki_der;
    GenerateRsaKey(key_size_bits_, &other_pkey, &other_key_spki_der);
    if (backend()->CreateUnsealingSession(
            sealed_secret_data, other_key_spki_der, supported_algorithms_,
            kPcrIndexesSet, delegate_blob_, delegate_secret_)) {
      LOG(ERROR)
          << "Error: unsealing session creation completed with a wrong key";
      return false;
    }
    return true;
  }

  bool CheckUnsealingFailsWithChangedPcrs(
      const SignatureSealedData& sealed_secret_data) {
    if (!tpm_->ExtendPCR(kPcrIndexToExtend,
                         SecureBlob("01234567890123456789"))) {
      LOG(ERROR) << "Error extending PCR";
      return false;
    }
    std::unique_ptr<UnsealingSession> unsealing_session(
        backend()->CreateUnsealingSession(sealed_secret_data, key_spki_der_,
                                          supported_algorithms_, kPcrIndexesSet,
                                          delegate_blob_, delegate_secret_));
    if (!unsealing_session) {
      LOG(ERROR) << "Error starting the unsealing session";
      return false;
    }
    SecureBlob challenge_signature;
    if (!SignWithKey(unsealing_session->GetChallengeValue(),
                     openssl_algorithm_nid_, &challenge_signature)) {
      LOG(ERROR) << "Error generating signature of challenge";
      return false;
    }
    SecureBlob unsealed_value;
    if (unsealing_session->Unseal(challenge_signature, &unsealed_value)) {
      LOG(ERROR) << "Error: unsealing completed with changed PCRs";
      return false;
    }
    return true;
  }

  bool SignWithKey(const SecureBlob& unhashed_data,
                   int algorithm_nid,
                   SecureBlob* signature) {
    signature->resize(EVP_PKEY_size(pkey_.get()));
    crypto::ScopedEVP_MD_CTX sign_context(EVP_MD_CTX_create());
    unsigned signature_size = 0;
    if (!sign_context ||
        !EVP_SignInit(sign_context.get(), EVP_get_digestbynid(algorithm_nid)) ||
        !EVP_SignUpdate(sign_context.get(), unhashed_data.data(),
                        unhashed_data.size()) ||
        !EVP_SignFinal(sign_context.get(), signature->data(), &signature_size,
                       pkey_.get())) {
      LOG(ERROR) << "Error signing data";
      return false;
    }
    signature->resize(signature_size);
    return true;
  }

  // Unowned.
  Tpm* const tpm_;
  const int key_size_bits_;
  const std::string test_case_description_;
  const std::vector<Algorithm> supported_algorithms_;
  const Algorithm expected_algorithm_;
  const int openssl_algorithm_nid_;
  const SecureBlob delegate_blob_;
  const SecureBlob delegate_secret_;
  crypto::ScopedEVP_PKEY pkey_;
  SecureBlob key_spki_der_;

  DISALLOW_COPY_AND_ASSIGN(SignatureSealedSecretTestCase);
};

}  // namespace

bool TpmLiveTest::SignatureSealedSecretTest() {
  using Algorithm = SignatureSealingBackend::Algorithm;
  if (!tpm_->GetSignatureSealingBackend()) {
    // Not supported by the Tpm implementation, just skip the test.
    return true;
  }
  VLOG(1) << "SignatureSealedSecretTest started";
  std::vector<SignatureSealedSecretTestCase> test_cases;
  for (int key_size_bits : {1024, 2048}) {
    test_cases.push_back(SignatureSealedSecretTestCase(
        tpm_, key_size_bits, "SHA-1", {Algorithm::kRsassaPkcs1V15Sha1},
        Algorithm::kRsassaPkcs1V15Sha1, NID_sha1));
    test_cases.push_back(SignatureSealedSecretTestCase(
        tpm_, key_size_bits, "SHA-256", {Algorithm::kRsassaPkcs1V15Sha256},
        Algorithm::kRsassaPkcs1V15Sha256, NID_sha256));
    test_cases.push_back(SignatureSealedSecretTestCase(
        tpm_, key_size_bits, "SHA-384", {Algorithm::kRsassaPkcs1V15Sha384},
        Algorithm::kRsassaPkcs1V15Sha384, NID_sha384));
    test_cases.push_back(SignatureSealedSecretTestCase(
        tpm_, key_size_bits, "SHA-512", {Algorithm::kRsassaPkcs1V15Sha512},
        Algorithm::kRsassaPkcs1V15Sha512, NID_sha512));
    test_cases.push_back(SignatureSealedSecretTestCase(
        tpm_, key_size_bits, "{SHA-384,SHA-256,SHA-512}",
        {Algorithm::kRsassaPkcs1V15Sha384, Algorithm::kRsassaPkcs1V15Sha256,
         Algorithm::kRsassaPkcs1V15Sha512},
        Algorithm::kRsassaPkcs1V15Sha384, NID_sha384));
    test_cases.push_back(SignatureSealedSecretTestCase(
        tpm_, key_size_bits, "{SHA-1,SHA-256}",
        {Algorithm::kRsassaPkcs1V15Sha1, Algorithm::kRsassaPkcs1V15Sha256},
        Algorithm::kRsassaPkcs1V15Sha256, NID_sha256));
  }
  for (auto& test_case : test_cases) {
    test_case.SetUp();
    if (!test_case.Run())
      return false;
  }
  VLOG(1) << "SignatureSealedSecretTest ended successfully.";
  return true;
}

}  // namespace cryptohome

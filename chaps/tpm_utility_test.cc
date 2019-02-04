// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes:
//  - Failed authentication is not tested because it can put the TPM in a state
//    where it refuses to perform authenticated operations for a period of time.
//  - Poorly formatted key blobs is not tested because they are not handled
//    correctly by Trousers and can crash the current process or tcsd.
#include "chaps/tpm_utility.h"

#include <memory>

#include <brillo/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include "chaps/chaps_utility.h"

#if USE_TPM2
#include "chaps/tpm2_utility_impl.h"
#else
#include "chaps/tpm_utility_impl.h"
#endif

using std::string;
using std::unique_ptr;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace chaps {

class TestTPMUtility: public ::testing::Test {
 public:
  TestTPMUtility() {
#if USE_TPM2
    // Instantiate a TPM2 Utility.
    tpm_.reset(new TPM2UtilityImpl());
#else
    // Instantiate a TPM1.2 Utility.
    tpm_.reset(new TPMUtilityImpl(""));
#endif
  }

  void SetUp() {
    size_ = 2048;
    e_ = string("\x1\x0\x1", 3);
    unsigned char random[20];
    RAND_bytes(random, 20);
    auth_ = brillo::SecureBlob(std::begin(random), std::end(random));
    EXPECT_TRUE(tpm_->Init());
  }

  void TestKey() {
    string e, n;
    EXPECT_TRUE(tpm_->GetRSAPublicKey(key_, &e, &n));
    EXPECT_EQ(n.length() * 8, size_);
    string input("input"), encrypted;
    EXPECT_TRUE(tpm_->Bind(key_, input, &encrypted));
    string input2;
    EXPECT_TRUE(tpm_->Unbind(key_, encrypted, &input2));
    EXPECT_TRUE(input == input2);
    string signature;
    EXPECT_TRUE(tpm_->Sign(key_, input, &signature));
    EXPECT_TRUE(tpm_->Verify(key_, input, signature));
  }

  bool InjectKey() {
    BIGNUM* e = ConvertToBIGNUM(e_);
    RSA* key = RSA_generate_key(size_, BN_get_word(e), NULL, NULL);
    string n = ConvertFromBIGNUM(key->n);
    string p = ConvertFromBIGNUM(key->p);
    bool result = tpm_->WrapRSAKey(0, e_, n, p, auth_, &blob_, &key_);
    RSA_free(key);
    BN_free(e);
    return result;
  }

 protected:
  unique_ptr<TPMUtility> tpm_;
  int size_;
  string e_;
  brillo::SecureBlob auth_;
  int key_ = 0;
  string blob_;
};

TEST_F(TestTPMUtility, Authenticate) {
  EXPECT_TRUE(InjectKey());
  // Setup for authentication.
  string master = "master_key";
  string encrypted_master;
  EXPECT_TRUE(tpm_->Bind(key_, master, &encrypted_master));
  // Successful authentication.
  brillo::SecureBlob master2;
  EXPECT_TRUE(tpm_->Authenticate(0, auth_, blob_, encrypted_master, &master2));
  EXPECT_TRUE(master == master2.to_string());
  tpm_->UnloadKeysForSlot(0);
  // Change password.
  unsigned char random[20];
  RAND_bytes(random, 20);
  brillo::SecureBlob auth2(std::begin(random), std::end(random));
  string blob2;
  EXPECT_TRUE(tpm_->ChangeAuthData(0, auth_, auth2, blob_, &blob2));
  tpm_->UnloadKeysForSlot(0);
  // Authenticate with new password.
  EXPECT_TRUE(tpm_->Authenticate(0, auth2, blob2, encrypted_master, &master2));
  EXPECT_TRUE(master == master2.to_string());
  tpm_->UnloadKeysForSlot(0);
}

TEST_F(TestTPMUtility, Random) {
  EXPECT_TRUE(tpm_->StirRandom("some_entropy"));
  string r;
  EXPECT_TRUE(tpm_->GenerateRandom(128, &r));
  EXPECT_EQ(128, r.length());
}

TEST_F(TestTPMUtility, GenerateRSAKey) {
  EXPECT_TRUE(tpm_->GenerateRSAKey(0, size_, e_, auth_, &blob_, &key_));
  TestKey();
  tpm_->UnloadKeysForSlot(0);
  EXPECT_TRUE(tpm_->LoadKey(0, blob_, auth_, &key_));
  TestKey();
  tpm_->UnloadKeysForSlot(0);
}

TEST_F(TestTPMUtility, WrappedKey) {
  EXPECT_TRUE(InjectKey());
  TestKey();
  tpm_->UnloadKeysForSlot(0);
  EXPECT_TRUE(tpm_->LoadKey(0, blob_, auth_, &key_));
  TestKey();
  // Test with some unexpected parameters.
  EXPECT_FALSE(
      tpm_->WrapRSAKey(0, e_, "invalid_n", "invalid_p", auth_, &blob_, &key_));
  tpm_->UnloadKeysForSlot(0);
}

TEST_F(TestTPMUtility, BadAuthSize) {
  EXPECT_TRUE(InjectKey());
  brillo::SecureBlob bad(48);
  brillo::SecureBlob tmp;
  string master("master"), encrypted;
  EXPECT_TRUE(tpm_->Bind(key_, master, &encrypted));
  tpm_->UnloadKeysForSlot(0);
  EXPECT_FALSE(tpm_->Authenticate(0, bad, blob_, encrypted, &tmp));
  EXPECT_FALSE(tpm_->GenerateRSAKey(0, size_, e_, bad, &blob_, &key_));
  tpm_->UnloadKeysForSlot(0);
  EXPECT_FALSE(tpm_->LoadKey(0, blob_, bad, &key_));
}

TEST_F(TestTPMUtility, BadKeyHandle) {
  int key = 17;
  string e, n;
  EXPECT_FALSE(tpm_->GetRSAPublicKey(key, &e, &n));
  string in, out;
  EXPECT_FALSE(tpm_->Unbind(key, in, &out));
  EXPECT_FALSE(tpm_->Sign(key, in, &out));
}

TEST_F(TestTPMUtility, BadInput) {
  const int max_plain = (size_ / 8) - 11;
  const int expected_encrypted = (size_ / 8);
  EXPECT_TRUE(InjectKey());
  string out;
  EXPECT_FALSE(tpm_->Bind(key_, string(max_plain + 1, 'a'), &out));
  EXPECT_TRUE(tpm_->Bind(key_, string(max_plain, 'a'), &out));
  EXPECT_EQ(expected_encrypted, out.length());
  EXPECT_FALSE(tpm_->Unbind(key_, out + string(1, 'a'), &out));
  tpm_->UnloadKeysForSlot(0);
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Notes:
//  - Failed authentication is not tested because it can put the TPM in a state
//    where it refuses to perform authenticated operations for a period of time.
//  - Poorly formatted key blobs is not tested because they are not handled
//    correctly by Trousers and can crash the current process or tcsd.
#include "tpm_utility_impl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>

#include "chaps/chaps_utility.h"

using std::string;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::SetArgumentPointee;

namespace chaps {

static string ConvertFromBIGNUM(const BIGNUM* bignum) {
  string big_integer(BN_num_bytes(bignum), 0);
  BN_bn2bin(bignum, ConvertStringToByteBuffer(big_integer.data()));
  return big_integer;
}

static BIGNUM* ConvertToBIGNUM(const string& big_integer) {
  BIGNUM* b = BN_bin2bn(ConvertStringToByteBuffer(big_integer.data()),
                        big_integer.length(),
                        NULL);
  return b;
}

class TestTPMUtility: public ::testing::Test {
 public:
  TestTPMUtility() : tpm_("") {}

  void SetUp() {
    size_ = 1024;
    e_ = string("\x1\x0\x1", 3);
    unsigned char random[20];
    RAND_bytes(random, 20);
    auth_ = string((char*)random, 20);
    ASSERT_TRUE(tpm_.Init());
  }

  void TestKey() {
    string e, n;
    EXPECT_TRUE(tpm_.GetPublicKey(key_, &e, &n));
    EXPECT_EQ(n.length() * 8, size_);
    EXPECT_TRUE(e == e_);
    string input("input"), encrypted;
    EXPECT_TRUE(tpm_.Bind(key_, input, &encrypted));
    string input2;
    EXPECT_TRUE(tpm_.Unbind(key_, encrypted, &input2));
    EXPECT_TRUE(input == input2);
    string signature;
    EXPECT_TRUE(tpm_.Sign(key_, input, &signature));
    EXPECT_TRUE(tpm_.Verify(key_, input, signature));
  }

  bool InjectKey() {
    BIGNUM* e = ConvertToBIGNUM(e_);
    RSA* key = RSA_generate_key(size_, BN_get_word(e), NULL, NULL);
    string n = ConvertFromBIGNUM(key->n);
    string p = ConvertFromBIGNUM(key->p);
    bool result = tpm_.WrapKey(0, e_, n, p, auth_, &blob_, &key_);
    RSA_free(key);
    BN_free(e);
    return result;
  }
 protected:
  TPMUtilityImpl tpm_;
  int size_;
  string e_;
  string auth_;
  int key_;
  string blob_;
};

TEST_F(TestTPMUtility, Authenticate) {
  ASSERT_TRUE(InjectKey());
  // Setup for authentication.
  string master = "master_key";
  string encrypted_master;
  ASSERT_TRUE(tpm_.Bind(key_, master, &encrypted_master));
  // Successful authentication.
  string master2;
  EXPECT_TRUE(tpm_.Authenticate(0, auth_, blob_, encrypted_master, &master2));
  EXPECT_TRUE(master == master2);
  tpm_.UnloadKeysForSlot(0);
  // Change password.
  unsigned char random[20];
  RAND_bytes(random, 20);
  string auth2((char*)random, 20);
  string blob2;
  ASSERT_TRUE(tpm_.ChangeAuthData(0, auth_, auth2, blob_, &blob2));
  tpm_.UnloadKeysForSlot(0);
  // Authenticate with new password.
  EXPECT_TRUE(tpm_.Authenticate(0, auth2, blob2, encrypted_master, &master2));
  EXPECT_TRUE(master == master2);
}

TEST_F(TestTPMUtility, Random) {
  EXPECT_TRUE(tpm_.StirRandom("some_entropy"));
  string r;
  EXPECT_TRUE(tpm_.GenerateRandom(128, &r));
  EXPECT_EQ(128, r.length());
}

TEST_F(TestTPMUtility, GenerateKey) {
  ASSERT_TRUE(tpm_.GenerateKey(0, size_, e_, auth_, &blob_, &key_));
  TestKey();
  tpm_.UnloadKeysForSlot(0);
  ASSERT_TRUE(tpm_.LoadKey(0, blob_, auth_, &key_));
  TestKey();
}

TEST_F(TestTPMUtility, WrappedKey) {
  ASSERT_TRUE(InjectKey());
  TestKey();
  tpm_.UnloadKeysForSlot(0);
  ASSERT_TRUE(tpm_.LoadKey(0, blob_, auth_, &key_));
  TestKey();
  // Test with some unexpected parameters.
  EXPECT_FALSE(tpm_.WrapKey(0, e_, "invalid_n", "invalid_p", auth_,
                            &blob_, &key_));
}

TEST_F(TestTPMUtility, BadAuthSize) {
  ASSERT_TRUE(InjectKey());
  string bad(19, 0);
  string master("master"), encrypted, tmp;
  ASSERT_TRUE(tpm_.Bind(key_, master, &encrypted));
  tpm_.UnloadKeysForSlot(0);
  EXPECT_FALSE(tpm_.Authenticate(0, bad, blob_, encrypted, &tmp));
  EXPECT_FALSE(tpm_.GenerateKey(0, size_, e_, bad, &blob_, &key_));
  tpm_.UnloadKeysForSlot(0);
  EXPECT_FALSE(tpm_.LoadKey(0, blob_, bad, &key_));
}

TEST_F(TestTPMUtility, BadKeyHandle) {
  int key = 17;
  string e, n;
  EXPECT_FALSE(tpm_.GetPublicKey(key, &e, &n));
  string in, out;
  EXPECT_FALSE(tpm_.Unbind(key, in, &out));
  EXPECT_FALSE(tpm_.Sign(key, in, &out));
}

TEST_F(TestTPMUtility, BadInput) {
  const int max_plain = (size_ / 8) - 11;
  const int expected_encrypted = (size_ / 8);
  ASSERT_TRUE(InjectKey());
  string out;
  EXPECT_FALSE(tpm_.Bind(key_, string(max_plain + 1, 'a'), &out));
  ASSERT_TRUE(tpm_.Bind(key_, string(max_plain, 'a'), &out));
  EXPECT_EQ(expected_encrypted, out.length());
  EXPECT_FALSE(tpm_.Unbind(key_, out + string(1, 'a'), &out));
  EXPECT_FALSE(tpm_.Sign(key_, string(max_plain + 1, 'a'), &out));
  ASSERT_TRUE(tpm_.Sign(key_, string(max_plain, 'a'), &out));
  EXPECT_EQ(expected_encrypted, out.length());
  EXPECT_FALSE(tpm_.Verify(key_, string(max_plain, 'a'), out + string(1, 'a')));
}

}  // namespace chaps

int main(int argc, char** argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

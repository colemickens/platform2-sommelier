// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "attestation/server/crypto_utility_impl.h"
#include "attestation/server/mock_tpm_utility.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace attestation {

class CryptoUtilityImplTest : public testing::Test {
 public:
  ~CryptoUtilityImplTest() override = default;
  void SetUp() override {
    crypto_utility_.reset(new CryptoUtilityImpl(&mock_tpm_utility_));
  }

 protected:
  NiceMock<MockTpmUtility> mock_tpm_utility_;
  std::unique_ptr<CryptoUtilityImpl> crypto_utility_;
};

TEST_F(CryptoUtilityImplTest, GetRandomSuccess) {
  std::string random1;
  EXPECT_TRUE(crypto_utility_->GetRandom(20, &random1));
  std::string random2;
  EXPECT_TRUE(crypto_utility_->GetRandom(20, &random2));
  EXPECT_NE(random1, random2);
}

TEST_F(CryptoUtilityImplTest, GetRandomIntOverflow) {
  size_t num_bytes = -1;
  std::string buffer;
  EXPECT_FALSE(crypto_utility_->GetRandom(num_bytes, &buffer));
}

TEST_F(CryptoUtilityImplTest, PairwiseSealedEncryption) {
  std::string key;
  std::string sealed_key;
  EXPECT_TRUE(crypto_utility_->CreateSealedKey(&key, &sealed_key));
  std::string data("test");
  std::string encrypted_data;
  EXPECT_TRUE(crypto_utility_->EncryptData(data, key, sealed_key,
                                           &encrypted_data));
  key.clear();
  sealed_key.clear();
  data.clear();
  EXPECT_TRUE(crypto_utility_->UnsealKey(encrypted_data, &key, &sealed_key));
  EXPECT_TRUE(crypto_utility_->DecryptData(encrypted_data, key, &data));
  EXPECT_EQ("test", data);
}

TEST_F(CryptoUtilityImplTest, SealFailure) {
  EXPECT_CALL(mock_tpm_utility_, SealToPCR0(_, _))
      .WillRepeatedly(Return(false));
  std::string key;
  std::string sealed_key;
  EXPECT_FALSE(crypto_utility_->CreateSealedKey(&key, &sealed_key));
}

TEST_F(CryptoUtilityImplTest, EncryptNoData) {
  std::string key(32, 0);
  std::string output;
  EXPECT_TRUE(crypto_utility_->EncryptData(std::string(), key, key, &output));
}

TEST_F(CryptoUtilityImplTest, EncryptInvalidKey) {
  std::string key(12, 0);
  std::string output;
  EXPECT_FALSE(crypto_utility_->EncryptData(std::string(), key, key, &output));
}

TEST_F(CryptoUtilityImplTest, UnsealInvalidData) {
  std::string output;
  EXPECT_FALSE(crypto_utility_->UnsealKey("invalid", &output, &output));
}

TEST_F(CryptoUtilityImplTest, UnsealError) {
  EXPECT_CALL(mock_tpm_utility_, Unseal(_, _))
      .WillRepeatedly(Return(false));
  std::string key(32, 0);
  std::string data;
  EXPECT_TRUE(crypto_utility_->EncryptData("data", key, key, &data));
  std::string output;
  EXPECT_FALSE(crypto_utility_->UnsealKey(data, &output, &output));
}

TEST_F(CryptoUtilityImplTest, DecryptInvalidKey) {
  std::string key(12, 0);
  std::string output;
  EXPECT_FALSE(crypto_utility_->DecryptData(std::string(), key, &output));
}

TEST_F(CryptoUtilityImplTest, DecryptInvalidData) {
  std::string key(32, 0);
  std::string output;
  EXPECT_FALSE(crypto_utility_->DecryptData("invalid", key, &output));
}

TEST_F(CryptoUtilityImplTest, DecryptInvalidData2) {
  std::string key(32, 0);
  std::string output;
  EncryptedData proto;
  std::string input;
  proto.SerializeToString(&input);
  EXPECT_FALSE(crypto_utility_->DecryptData(input, key, &output));
}

}  // namespace attestation

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/privet/security_manager.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <chromeos/data_encoding.h>
#include <chromeos/key_value_store.h>
#include <chromeos/message_loops/fake_message_loop.h>
#include <chromeos/strings/string_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "libweave/external/crypto/p224_spake.h"
#include "libweave/src/privet/openssl_utils.h"

using testing::Eq;
using testing::_;

namespace weave {
namespace privet {

namespace {

bool IsBase64Char(char c) {
  return isalnum(c) || (c == '+') || (c == '/') || (c == '=');
}

bool IsBase64(const std::string& text) {
  return !text.empty() &&
         !std::any_of(text.begin(), text.end(),
                      std::not1(std::ref(IsBase64Char)));
}

class MockPairingCallbacks {
 public:
  MOCK_METHOD3(OnPairingStart,
               void(const std::string& session_id,
                    PairingType pairing_type,
                    const std::vector<uint8_t>& code));
  MOCK_METHOD1(OnPairingEnd, void(const std::string& session_id));
};

base::FilePath GetTempFilePath() {
  base::FilePath file_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&file_path));
  return file_path;
}

}  // namespace

class SecurityManagerTest : public testing::Test {
 public:
  void SetUp() override {
    chromeos::Blob fingerprint;
    fingerprint.resize(256 / 8);
    base::RandBytes(fingerprint.data(), fingerprint.size());
    security_.SetCertificateFingerprint(fingerprint);

    chromeos::KeyValueStore store;
    store.SetString("embedded_code", "1234");
    EXPECT_TRUE(store.Save(embedded_code_path_));
  }

  void TearDown() override { base::DeleteFile(embedded_code_path_, false); }

 protected:
  void PairAndAuthenticate(std::string* fingerprint, std::string* signature) {
    std::string session_id;
    std::string device_commitment_base64;

    EXPECT_TRUE(security_.StartPairing(PairingType::kEmbeddedCode,
                                       CryptoType::kSpake_p224, &session_id,
                                       &device_commitment_base64, nullptr));
    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(device_commitment_base64.empty());

    crypto::P224EncryptedKeyExchange spake{
        crypto::P224EncryptedKeyExchange::kPeerTypeClient, "1234"};

    std::string client_commitment_base64{
        chromeos::data_encoding::Base64Encode(spake.GetNextMessage())};

    EXPECT_TRUE(security_.ConfirmPairing(session_id, client_commitment_base64,
                                         fingerprint, signature, nullptr));
    EXPECT_TRUE(IsBase64(*fingerprint));
    EXPECT_TRUE(IsBase64(*signature));

    chromeos::Blob device_commitment;
    ASSERT_TRUE(chromeos::data_encoding::Base64Decode(device_commitment_base64,
                                                      &device_commitment));
    spake.ProcessMessage(
        chromeos::string_utils::GetBytesAsString(device_commitment));

    chromeos::Blob auth_code{
        HmacSha256(chromeos::SecureBlob{spake.GetUnverifiedKey()},
                   chromeos::SecureBlob{session_id})};

    std::string auth_code_base64{
        chromeos::data_encoding::Base64Encode(auth_code)};

    EXPECT_TRUE(security_.IsValidPairingCode(auth_code_base64));
  }

  const base::Time time_ = base::Time::FromTimeT(1410000000);
  base::FilePath embedded_code_path_{GetTempFilePath()};
  base::SimpleTestClock clock_;
  chromeos::FakeMessageLoop task_runner_{&clock_};
  SecurityManager security_{{PairingType::kEmbeddedCode},
                            embedded_code_path_,
                            &task_runner_,
                            false};
};

TEST_F(SecurityManagerTest, IsBase64) {
  EXPECT_TRUE(IsBase64(
      security_.CreateAccessToken(UserInfo{AuthScope::kUser, 7}, time_)));
}

TEST_F(SecurityManagerTest, CreateSameToken) {
  EXPECT_EQ(
      security_.CreateAccessToken(UserInfo{AuthScope::kViewer, 555}, time_),
      security_.CreateAccessToken(UserInfo{AuthScope::kViewer, 555}, time_));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentScope) {
  EXPECT_NE(
      security_.CreateAccessToken(UserInfo{AuthScope::kViewer, 456}, time_),
      security_.CreateAccessToken(UserInfo{AuthScope::kOwner, 456}, time_));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentUser) {
  EXPECT_NE(
      security_.CreateAccessToken(UserInfo{AuthScope::kOwner, 456}, time_),
      security_.CreateAccessToken(UserInfo{AuthScope::kOwner, 789}, time_));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentTime) {
  EXPECT_NE(
      security_.CreateAccessToken(UserInfo{AuthScope::kOwner, 567}, time_),
      security_.CreateAccessToken(UserInfo{AuthScope::kOwner, 567},
                                  base::Time::FromTimeT(1400000000)));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentInstance) {
  EXPECT_NE(security_.CreateAccessToken(UserInfo{AuthScope::kUser, 123}, time_),
            SecurityManager({}, base::FilePath{}, &task_runner_, false)
                .CreateAccessToken(UserInfo{AuthScope::kUser, 123}, time_));
}

TEST_F(SecurityManagerTest, ParseAccessToken) {
  // Multiple attempts with random secrets.
  for (size_t i = 0; i < 1000; ++i) {
    SecurityManager security{{}, base::FilePath{}, &task_runner_, false};

    std::string token =
        security.CreateAccessToken(UserInfo{AuthScope::kUser, 5}, time_);
    base::Time time2;
    EXPECT_EQ(AuthScope::kUser,
              security.ParseAccessToken(token, &time2).scope());
    EXPECT_EQ(5u, security.ParseAccessToken(token, &time2).user_id());
    // Token timestamp resolution is one second.
    EXPECT_GE(1, std::abs((time_ - time2).InSeconds()));
  }
}

TEST_F(SecurityManagerTest, PairingNoSession) {
  std::string fingerprint;
  std::string signature;
  chromeos::ErrorPtr error;
  ASSERT_FALSE(
      security_.ConfirmPairing("123", "345", &fingerprint, &signature, &error));
  EXPECT_EQ("unknownSession", error->GetCode());
}

TEST_F(SecurityManagerTest, Pairing) {
  std::vector<std::pair<std::string, std::string> > fingerprints(2);
  for (auto& it : fingerprints) {
    PairAndAuthenticate(&it.first, &it.second);
  }

  // Same certificate.
  EXPECT_EQ(fingerprints.front().first, fingerprints.back().first);

  // Signed with different secret.
  EXPECT_NE(fingerprints.front().second, fingerprints.back().second);
}

TEST_F(SecurityManagerTest, NotifiesListenersOfSessionStartAndEnd) {
  testing::StrictMock<MockPairingCallbacks> callbacks;
  security_.RegisterPairingListeners(
      base::Bind(&MockPairingCallbacks::OnPairingStart,
                 base::Unretained(&callbacks)),
      base::Bind(&MockPairingCallbacks::OnPairingEnd,
                 base::Unretained(&callbacks)));
  for (auto commitment_suffix :
       std::vector<std::string>{"", "invalid_commitment"}) {
    // StartPairing should notify us that a new session has begun.
    std::string session_id;
    std::string device_commitment;
    EXPECT_CALL(callbacks, OnPairingStart(_, PairingType::kEmbeddedCode, _));
    EXPECT_TRUE(security_.StartPairing(PairingType::kEmbeddedCode,
                                       CryptoType::kSpake_p224, &session_id,
                                       &device_commitment, nullptr));
    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(device_commitment.empty());
    testing::Mock::VerifyAndClearExpectations(&callbacks);

    // ConfirmPairing should notify us that the session has ended.
    EXPECT_CALL(callbacks, OnPairingEnd(Eq(session_id)));
    crypto::P224EncryptedKeyExchange spake{
        crypto::P224EncryptedKeyExchange::kPeerTypeServer, "1234"};
    std::string client_commitment =
        chromeos::data_encoding::Base64Encode(spake.GetNextMessage());
    std::string fingerprint, signature;
    // Regardless of whether the commitment is valid or not, we should get a
    // callback indicating that the pairing session is gone.
    security_.ConfirmPairing(session_id, client_commitment + commitment_suffix,
                             &fingerprint, &signature, nullptr);
    testing::Mock::VerifyAndClearExpectations(&callbacks);
  }
}

TEST_F(SecurityManagerTest, CancelPairing) {
  testing::StrictMock<MockPairingCallbacks> callbacks;
  security_.RegisterPairingListeners(
      base::Bind(&MockPairingCallbacks::OnPairingStart,
                 base::Unretained(&callbacks)),
      base::Bind(&MockPairingCallbacks::OnPairingEnd,
                 base::Unretained(&callbacks)));
  std::string session_id;
  std::string device_commitment;
  EXPECT_CALL(callbacks, OnPairingStart(_, PairingType::kEmbeddedCode, _));
  EXPECT_TRUE(security_.StartPairing(PairingType::kEmbeddedCode,
                                     CryptoType::kSpake_p224, &session_id,
                                     &device_commitment, nullptr));
  EXPECT_CALL(callbacks, OnPairingEnd(Eq(session_id)));
  EXPECT_TRUE(security_.CancelPairing(session_id, nullptr));
}

TEST_F(SecurityManagerTest, ThrottlePairing) {
  auto pair = [this]() {
    std::string session_id;
    std::string device_commitment;
    chromeos::ErrorPtr error;
    bool result = security_.StartPairing(PairingType::kEmbeddedCode,
                                         CryptoType::kSpake_p224, &session_id,
                                         &device_commitment, &error);
    EXPECT_TRUE(result || error->GetCode() == "deviceBusy");
    return result;
  };

  EXPECT_TRUE(pair());
  EXPECT_TRUE(pair());
  EXPECT_TRUE(pair());
  EXPECT_FALSE(pair());
  EXPECT_GT(security_.block_pairing_until_, base::Time::Now());
  EXPECT_LE(security_.block_pairing_until_,
            base::Time::Now() + base::TimeDelta::FromMinutes(15));

  // Wait timeout.
  security_.block_pairing_until_ =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);

  // Allow exactly one attempt.
  EXPECT_TRUE(pair());
  EXPECT_FALSE(pair());

  // Wait timeout.
  security_.block_pairing_until_ =
      base::Time::Now() - base::TimeDelta::FromMinutes(1);

  // Completely unblock by successfully pairing.
  std::string fingerprint;
  std::string signature;
  PairAndAuthenticate(&fingerprint, &signature);

  // Now we have 3 attempts again.
  EXPECT_TRUE(pair());
  EXPECT_TRUE(pair());
  EXPECT_TRUE(pair());
  EXPECT_FALSE(pair());
}

TEST_F(SecurityManagerTest, DontBlockForCanceledSessions) {
  for (int i = 0; i < 20; ++i) {
    std::string session_id;
    std::string device_commitment;
    EXPECT_TRUE(security_.StartPairing(PairingType::kEmbeddedCode,
                                       CryptoType::kSpake_p224, &session_id,
                                       &device_commitment, nullptr));
    EXPECT_TRUE(security_.CancelPairing(session_id, nullptr));
  }
}

TEST_F(SecurityManagerTest, EmbeddedCodeNotReady) {
  std::string session_id;
  std::string device_commitment;
  base::DeleteFile(embedded_code_path_, false);
  chromeos::ErrorPtr error;
  ASSERT_FALSE(security_.StartPairing(PairingType::kEmbeddedCode,
                                      CryptoType::kSpake_p224, &session_id,
                                      &device_commitment, &error));
  EXPECT_EQ("deviceBusy", error->GetCode());
}

}  // namespace privet
}  // namespace weave

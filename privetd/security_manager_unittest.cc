// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/security_manager.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

namespace privetd {

namespace {

bool IsBase64Char(char c) {
  return isalnum(c) || (c == '+') || (c == '/') || (c == '=');
}

bool IsBase64(const std::string& text) {
  return !text.empty() &&
         !std::any_of(text.begin(), text.end(),
                      std::not1(std::ref(IsBase64Char)));
}

}  // namespace

class SecurityManagerTest : public testing::Test {
 protected:
  const base::Time time_ = base::Time::FromTimeT(1410000000);
  SecurityManager security_;
};

TEST_F(SecurityManagerTest, IsBase64) {
  EXPECT_TRUE(IsBase64(security_.CreateAccessToken(AuthScope::kGuest, time_)));
}

TEST_F(SecurityManagerTest, CreateSameToken) {
  EXPECT_EQ(security_.CreateAccessToken(AuthScope::kGuest, time_),
            security_.CreateAccessToken(AuthScope::kGuest, time_));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentScope) {
  EXPECT_NE(security_.CreateAccessToken(AuthScope::kGuest, time_),
            security_.CreateAccessToken(AuthScope::kOwner, time_));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentTime) {
  EXPECT_NE(security_.CreateAccessToken(AuthScope::kGuest, time_),
            security_.CreateAccessToken(AuthScope::kGuest,
                                        base::Time::FromTimeT(1400000000)));
}

TEST_F(SecurityManagerTest, CreateTokenDifferentInstance) {
  EXPECT_NE(security_.CreateAccessToken(AuthScope::kGuest, time_),
            SecurityManager().CreateAccessToken(AuthScope::kGuest, time_));
}

TEST_F(SecurityManagerTest, ParseAccessToken) {
  // Multiple attempts with random secrets.
  for (size_t i = 0; i < 1000; ++i) {
    SecurityManager security;
    std::string token = security.CreateAccessToken(AuthScope::kUser, time_);
    base::Time time2;
    EXPECT_EQ(AuthScope::kUser, security.ParseAccessToken(token, &time2));
    // Token timestamp resolution is one second.
    EXPECT_GE(1, std::abs((time_ - time2).InSeconds()));
  }
}

TEST_F(SecurityManagerTest, TlsData) {
  security_.InitTlsData();

  std::string key_str = security_.GetTlsPrivateKey().to_string();
  EXPECT_TRUE(
      StartsWithASCII(key_str, "-----BEGIN RSA PRIVATE KEY-----", false));
  EXPECT_TRUE(EndsWith(key_str, "-----END RSA PRIVATE KEY-----\n", false));

  const chromeos::Blob& cert = security_.GetTlsCertificate();
  std::string cert_str(cert.begin(), cert.end());
  EXPECT_TRUE(StartsWithASCII(cert_str, "-----BEGIN CERTIFICATE-----", false));
  EXPECT_TRUE(EndsWith(cert_str, "-----END CERTIFICATE-----\n", false));
}

TEST_F(SecurityManagerTest, PairingNoSession) {
  std::string fingerprint;
  std::string signature;
  EXPECT_EQ(Error::kUnknownSession,
            security_.ConfirmPairing("123", "345", &fingerprint, &signature));
}

TEST_F(SecurityManagerTest, Pairing) {
  security_.InitTlsData();

  std::string session_id;
  std::string device_commitment;
  EXPECT_EQ(Error::kNone,
            security_.StartPairing(PairingType::kEmbeddedCode, &session_id,
                                   &device_commitment));
  EXPECT_FALSE(session_id.empty());
  EXPECT_FALSE(device_commitment.empty());

  std::string fingerprint1;
  std::string signature1;
  EXPECT_EQ(Error::kNone, security_.ConfirmPairing(session_id, "345",
                                                   &fingerprint1, &signature1));
  EXPECT_TRUE(IsBase64(fingerprint1));
  EXPECT_TRUE(IsBase64(signature1));

  std::string fingerprint2;
  std::string signature2;
  EXPECT_EQ(Error::kNone, security_.ConfirmPairing(session_id, "678",
                                                   &fingerprint2, &signature2));
  EXPECT_TRUE(IsBase64(fingerprint2));
  EXPECT_TRUE(IsBase64(signature2));

  EXPECT_EQ(fingerprint1, fingerprint2);  // Same certificate.
  EXPECT_NE(signature1, signature2);      // Signed with different secret.
}

}  // namespace privetd

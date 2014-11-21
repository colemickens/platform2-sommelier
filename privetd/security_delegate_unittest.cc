// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/security_delegate.h"

#include <algorithm>
#include <memory>

#include <base/logging.h>
#include <base/strings/string_util.h>
#include <base/strings/string_number_conversions.h>
#include <gtest/gtest.h>

namespace privetd {

class SecurityDelegateTest : public testing::Test {
 protected:
  void SetUp() override { security_ = SecurityDelegate::CreateDefault(); }

  const base::Time time_ = base::Time::FromTimeT(1410000000);
  std::unique_ptr<SecurityDelegate> security_;
};

TEST_F(SecurityDelegateTest, IsBase64) {
     for (char c : security_->CreateAccessToken(AuthScope::kGuest, time_)) {
    EXPECT_TRUE(isalnum(c) || (c == '+') || (c == '/') || (c == '='));
  }
}

TEST_F(SecurityDelegateTest, CreateSameToken) {
  EXPECT_EQ(security_->CreateAccessToken(AuthScope::kGuest, time_),
            security_->CreateAccessToken(AuthScope::kGuest, time_));
}

TEST_F(SecurityDelegateTest, CreateTokenDifferentScope) {
  EXPECT_NE(security_->CreateAccessToken(AuthScope::kGuest, time_),
            security_->CreateAccessToken(AuthScope::kOwner, time_));
}

TEST_F(SecurityDelegateTest, CreateTokenDifferentTime) {
  EXPECT_NE(security_->CreateAccessToken(AuthScope::kGuest, time_),
            security_->CreateAccessToken(AuthScope::kGuest,
                                         base::Time::FromTimeT(1400000000)));
}

TEST_F(SecurityDelegateTest, CreateTokenDifferentInstance) {
  EXPECT_NE(security_->CreateAccessToken(AuthScope::kGuest, time_),
            SecurityDelegate::CreateDefault()->CreateAccessToken(
                AuthScope::kGuest, time_));
}

TEST_F(SecurityDelegateTest, ParseAccessToken) {
  // Multiple attempts with random secrets.
  for (size_t i = 0; i < 1000; ++i) {
    security_ = SecurityDelegate::CreateDefault();
    std::string token = security_->CreateAccessToken(AuthScope::kUser, time_);
    base::Time time2;
    EXPECT_EQ(AuthScope::kUser, security_->ParseAccessToken(token, &time2));
    // Token timestamp resolution is one second.
    EXPECT_GE(1, std::abs((time_ - time2).InSeconds()));
  }
}

TEST_F(SecurityDelegateTest, TlsData) {
  security_->InitTlsData();

  std::string key_str = security_->GetTlsPrivateKey().to_string();
  EXPECT_TRUE(
      StartsWithASCII(key_str, "-----BEGIN RSA PRIVATE KEY-----", false));
  EXPECT_TRUE(EndsWith(key_str, "-----END RSA PRIVATE KEY-----\n", false));

  const chromeos::Blob& cert = security_->GetTlsCertificate();
  std::string cert_str(cert.begin(), cert.end());
  EXPECT_TRUE(StartsWithASCII(cert_str, "-----BEGIN CERTIFICATE-----", false));
  EXPECT_TRUE(EndsWith(cert_str, "-----END CERTIFICATE-----\n", false));
}

}  // namespace privetd

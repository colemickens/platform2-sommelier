// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/smdp_fi_impl.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;

namespace hermes {

class SmdpFiTester {
 public:
  MOCK_METHOD1(OnInitiateAuth, void(const std::vector<uint8_t>& data));
  MOCK_METHOD1(OnAuthClient, void(const std::vector<uint8_t>& data));
  MOCK_METHOD1(FakeError, void(const std::vector<uint8_t>& error_data));
};

class SmdpFiImplTest : public testing::Test {
 public:
  SmdpFiImplTest() = default;
  ~SmdpFiImplTest() = default;

 protected:
  SmdpFiTester smdp_tester_;
  SmdpFiImpl smdp_;
};

TEST_F(SmdpFiImplTest, InitiateAuthenticationTest) {
  const std::vector<uint8_t> fake_info1 = {0x00, 0x01};
  const std::vector<uint8_t> fake_challenge = {0x02, 0x03};
  EXPECT_CALL(smdp_tester_, OnInitiateAuth(_)).Times(1);
  EXPECT_CALL(smdp_tester_, FakeError(_)).Times(0);

  smdp_.InitiateAuthentication(
      fake_info1, fake_challenge,
      base::Bind(&SmdpFiTester::OnInitiateAuth,
                 base::Unretained(&smdp_tester_)),
      base::Bind(&SmdpFiTester::FakeError, base::Unretained(&smdp_tester_)));
}

TEST_F(SmdpFiImplTest, AuthenticateClientTest) {
  const std::vector<uint8_t> esim_data = {0, 1, 2, 3, 4};
  EXPECT_CALL(smdp_tester_, OnAuthClient(_)).Times(1);
  EXPECT_CALL(smdp_tester_, FakeError(_)).Times(0);

  smdp_.AuthenticateClient(
      esim_data,
      base::Bind(&SmdpFiTester::OnAuthClient, base::Unretained(&smdp_tester_)),
      base::Bind(&SmdpFiTester::FakeError, base::Unretained(&smdp_tester_)));
}

}  // namespace hermes

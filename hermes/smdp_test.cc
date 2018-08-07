// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/smdp_impl.h"

#include <base/bind.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;

namespace hermes {

class SmdpFiTester {
 public:
  MOCK_METHOD5(OnInitiateAuth, void(const std::string&,
                                    const std::vector<uint8_t>&,
                                    const std::vector<uint8_t>&,
                                    const std::vector<uint8_t>&,
                                    const std::vector<uint8_t>&));
  MOCK_METHOD5(OnAuthClient, void(const std::string&,
                                  const std::vector<uint8_t>&,
                                  const std::vector<uint8_t>&,
                                  const std::vector<uint8_t>&,
                                  const std::vector<uint8_t>&));
  MOCK_METHOD1(FakeError, void(const std::vector<uint8_t>& error_data));
};

class SmdpImplTest : public testing::Test {
 public:
  SmdpImplTest() : smdp_("") {}
  ~SmdpImplTest() = default;

 protected:
  SmdpFiTester smdp_tester_;
  SmdpImpl smdp_;
};

TEST_F(SmdpImplTest, InitiateAuthenticationTest) {
  const std::vector<uint8_t> fake_info1 = {0x00, 0x01};
  const std::vector<uint8_t> fake_challenge = {0x02, 0x03};
  EXPECT_CALL(smdp_tester_, OnInitiateAuth(_, _, _, _, _)).Times(1);
  EXPECT_CALL(smdp_tester_, FakeError(_)).Times(0);

  smdp_.InitiateAuthentication(
      fake_info1, fake_challenge,
      base::Bind(&SmdpFiTester::OnInitiateAuth,
                 base::Unretained(&smdp_tester_)),
      base::Bind(&SmdpFiTester::FakeError, base::Unretained(&smdp_tester_)));
}

TEST_F(SmdpImplTest, AuthenticateClientTest) {
  const std::string transaction_id = "1";
  const std::vector<uint8_t> esim_data = {0, 1, 2, 3, 4};
  EXPECT_CALL(smdp_tester_, OnAuthClient(_, _, _, _, _)).Times(1);
  EXPECT_CALL(smdp_tester_, FakeError(_)).Times(0);

  smdp_.AuthenticateClient(
    transaction_id,
    esim_data,
    base::Bind(&SmdpFiTester::OnAuthClient, base::Unretained(&smdp_tester_)),
    base::Bind(&SmdpFiTester::FakeError, base::Unretained(&smdp_tester_)));
}

}  // namespace hermes

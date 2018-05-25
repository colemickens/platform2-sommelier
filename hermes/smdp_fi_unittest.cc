// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/smdp_fi_impl.h"

#include <memory>

#include <base/bind.h>
#include <gtest/gtest.h>

namespace hermes {

class SmdpFiImplTest : public testing::Test {
 public:
  SmdpFiImplTest() = default;
  ~SmdpFiImplTest() = default;

  void InitiateAuthCallback(const std::vector<uint8_t>& data) {
    return_data_ = data;
  }

  void InitiateAuthErrorCallback(const std::vector<uint8_t>& error_data) {}

  void AuthClientCallback(const std::vector<uint8_t>& data) {
    return_data_ = data;
  }

  void AuthClientErrorCallback(const std::vector<uint8_t>& error_data) {}

 protected:
  SmdpFiImpl smdp_;
  std::vector<uint8_t> return_data_;
};

TEST_F(SmdpFiImplTest, InitiateAuthenticationTest) {
  const std::vector<uint8_t> info1 = {1, 2, 3, 4, 5};
  const std::vector<uint8_t> chal = {6, 7, 8, 9, 0};
  const std::vector<uint8_t> expected = {5, 10, 15, 20, 25};
  smdp_.InitiateAuthentication(
      info1, chal,
      base::Bind(&SmdpFiImplTest::InitiateAuthCallback, base::Unretained(this)),
      base::Bind(&SmdpFiImplTest::InitiateAuthErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(return_data_, expected);
}

TEST_F(SmdpFiImplTest, AuthenticateClientTest) {
  const std::vector<uint8_t> esim_data = {0, 1, 2, 3, 4};
  const std::vector<uint8_t> expected = {2, 4, 6, 8, 10};
  smdp_.AuthenticateClient(
      esim_data,
      base::Bind(&SmdpFiImplTest::AuthClientCallback, base::Unretained(this)),
      base::Bind(&SmdpFiImplTest::AuthClientErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(return_data_, expected);
}

}  // namespace hermes

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_qmi_impl.h"

#include <base/bind.h>
#include <gtest/gtest.h>

namespace {
constexpr uint16_t kEuiccInfo1 = 0xBF20;
}  // namespace

namespace hermes {

class EsimQmiImplTest : public testing::Test {
 public:
  EsimQmiImplTest() = default;
  ~EsimQmiImplTest() = default;
  void InfoResult(const std::vector<uint8_t>& data) { return_data_ = data; }

  void ChallengeResult(const std::vector<uint8_t>& info1,
                       const std::vector<uint8_t>& challenge) {
    return_data_ = challenge;
  }

  void Error(const std::vector<uint8_t>& data) { return_data_ = data; }

 protected:
  EsimQmiImpl esim_;
  std::vector<uint8_t> return_data_;
};

TEST_F(EsimQmiImplTest, GetEuiccInfoTest) {
  const std::vector<uint8_t> expected_info = {1, 2, 3, 4, 5};
  esim_.GetInfo(
      kEuiccInfo1,
      base::Bind(&EsimQmiImplTest::InfoResult, base::Unretained(this)),
      base::Bind(&EsimQmiImplTest::Error, base::Unretained(this)));
  EXPECT_EQ(return_data_, expected_info);
}

TEST_F(EsimQmiImplTest, GetEuiccChallengeTest) {
  const std::vector<uint8_t> zero = {0};
  const std::vector<uint8_t> expected_challenge = {0x10, 0x11, 0x12,
                                                   0x13, 0x14, 0x15};
  esim_.GetChallenge(
      base::Bind(&EsimQmiImplTest::ChallengeResult, base::Unretained(this),
                 zero),
      base::Bind(&EsimQmiImplTest::Error, base::Unretained(this)));

  EXPECT_EQ(return_data_, expected_challenge);
}

}  // namespace hermes

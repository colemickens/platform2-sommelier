// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim_qmi_impl.h"

#include <base/bind.h>
#include <base/callback.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hermes/qmi_constants.h"

using ::testing::_;

namespace hermes {

class EsimQmiTester {
 public:
  MOCK_METHOD1(OnInfoResult, void(const std::vector<uint8_t>& data));
  MOCK_METHOD1(OnChallengeResult, void(const std::vector<uint8_t>& data));
  MOCK_METHOD1(FakeError, void(const std::vector<uint8_t>& error_data));
};

class EsimQmiImplTest : public testing::Test {
 public:
  EsimQmiImplTest() = default;
  ~EsimQmiImplTest() = default;

 protected:
  EsimQmiTester esim_tester_;
  EsimQmiImpl esim_;
};

// Test simple functionality of EsimQmiImpl::GetInfo. This test passes if
// EsimQmiTester::OnInfoResult gets called once and EsimQmiTester::FakeError
// does not get called.
TEST_F(EsimQmiImplTest, GetInfoTest) {
  EXPECT_CALL(esim_tester_, OnInfoResult(_)).Times(1);
  EXPECT_CALL(esim_tester_, FakeError(_)).Times(0);

  esim_.GetInfo(
      kEsimInfo1,
      base::Bind(&EsimQmiTester::OnInfoResult, base::Unretained(&esim_tester_)),
      base::Bind(&EsimQmiTester::FakeError, base::Unretained(&esim_tester_)));
}

// Test simple functionality of EsimQmiImpl::GetChallenge. This test passes if
// EsimQmiTester::OnChallengeResult gets called once and
// EsimQmiTester::FakeError does not get called.
TEST_F(EsimQmiImplTest, GetChallengeTest) {
  EXPECT_CALL(esim_tester_, OnChallengeResult(_)).Times(1);
  EXPECT_CALL(esim_tester_, FakeError(_)).Times(0);

  esim_.GetChallenge(
      base::Bind(&EsimQmiTester::OnChallengeResult,
                 base::Unretained(&esim_tester_)),
      base::Bind(&EsimQmiTester::FakeError, base::Unretained(&esim_tester_)));
}

}  // namespace hermes

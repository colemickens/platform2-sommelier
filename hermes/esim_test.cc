// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/esim.h"

#include <base/bind.h>
#include <base/callback.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "hermes/qmi_constants.h"

using ::testing::_;

namespace hermes {

class EsimTester {
 public:
  MOCK_METHOD1(OnInfoResult, void(const std::vector<uint8_t>& data));
  MOCK_METHOD2(OnChallengeResult,
               void(const std::vector<uint8_t>& info,
                    const std::vector<uint8_t>& challenge));
  MOCK_METHOD1(FakeError, void(EsimError error));
};

class EsimTest : public testing::Test {
 public:
  EsimTest() {
    esim_ = Esim::CreateForTest(&fd_, "", "");
    esim_->Initialize(
        base::Bind(&EsimTest::FakeSuccess, base::Unretained(this)),
        base::Bind(&EsimTest::FakeError, base::Unretained(this)));
  }
  ~EsimTest() = default;

  void FakeSuccess() {}
  void FakeError(EsimError error) {}

 protected:
  base::ScopedFD fd_;
  EsimTester esim_tester_;
  std::unique_ptr<Esim> esim_;
};

// Test simple functionality of Esim::GetInfo. This test passes if
// EsimQmiTester::OnInfoResult gets called once and EsimQmiTester::FakeError
// does not get called.
TEST_F(EsimTest, GetInfoTest) {
  EXPECT_CALL(esim_tester_, OnInfoResult(_)).Times(1);

  // This should call OnInfoResult
  esim_->GetInfo(
      kEsimInfo1Tag,
      base::Bind(&EsimTester::OnInfoResult, base::Unretained(&esim_tester_)),
      base::Bind(&EsimTester::FakeError, base::Unretained(&esim_tester_)));

  testing::Mock::VerifyAndClearExpectations(&esim_tester_);
  EXPECT_CALL(esim_tester_, FakeError(EsimError::kEsimError)).Times(1);

  // This should call FakeError
  esim_->GetInfo(
      0,
      base::Bind(&EsimTester::OnInfoResult, base::Unretained(&esim_tester_)),
      base::Bind(&EsimTester::FakeError, base::Unretained(&esim_tester_)));
}

// Test simple functionality of Esim::GetChallenge. This test passes if
// EsimQmiTester::OnChallengeResult gets called once and
// EsimQmiTester::FakeError does not get called.
TEST_F(EsimTest, GetChallengeTest) {
  const std::vector<uint8_t> info = {0x00, 0x01, 0x02, 0x03};
  EXPECT_CALL(esim_tester_, OnChallengeResult(info, _)).Times(1);
  EXPECT_CALL(esim_tester_, FakeError(_)).Times(0);

  // This should call OnChallengeResult
  esim_->GetChallenge(
      base::Bind(&EsimTester::OnChallengeResult,
                 base::Unretained(&esim_tester_), info),
      base::Bind(&EsimTester::FakeError, base::Unretained(&esim_tester_)));
}

}  // namespace hermes

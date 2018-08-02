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
  MOCK_METHOD1(OnInfoResult, void(const Esim::DataBlob& data));
  MOCK_METHOD2(OnChallengeResult,
               void(const Esim::DataBlob& info,
                    const Esim::DataBlob& challenge));
  MOCK_METHOD1(FakeError, void(EsimError error));
};

class EsimQmiImplTest : public testing::Test {
 public:
  EsimQmiImplTest() {
    esim_ = EsimQmiImpl::CreateForTest(&fd_);
    esim_->Initialize(
        base::Bind(&EsimQmiImplTest::FakeSuccess, base::Unretained(this)),
        base::Bind(&EsimQmiImplTest::FakeError, base::Unretained(this)));
  }
  ~EsimQmiImplTest() = default;

  void FakeSuccess() {}
  void FakeError(EsimError error) {}

 protected:
  base::ScopedFD fd_;
  EsimQmiTester esim_tester_;
  std::unique_ptr<EsimQmiImpl> esim_;
};

// Test simple functionality of EsimQmiImpl::GetInfo. This test passes if
// EsimQmiTester::OnInfoResult gets called once and EsimQmiTester::FakeError
// does not get called.
TEST_F(EsimQmiImplTest, GetInfoTest) {
  EXPECT_CALL(esim_tester_, OnInfoResult(_)).Times(1);

  // This should call OnInfoResult
  esim_->GetInfo(
      kEsimInfo1Tag,
      base::Bind(&EsimQmiTester::OnInfoResult, base::Unretained(&esim_tester_)),
      base::Bind(&EsimQmiTester::FakeError, base::Unretained(&esim_tester_)));

  testing::Mock::VerifyAndClearExpectations(&esim_tester_);
  EXPECT_CALL(esim_tester_, FakeError(EsimError::kEsimError)).Times(1);

  // This should call FakeError
  esim_->GetInfo(
      0,
      base::Bind(&EsimQmiTester::OnInfoResult, base::Unretained(&esim_tester_)),
      base::Bind(&EsimQmiTester::FakeError, base::Unretained(&esim_tester_)));
}

// Test simple functionality of EsimQmiImpl::GetChallenge. This test passes if
// EsimQmiTester::OnChallengeResult gets called once and
// EsimQmiTester::FakeError does not get called.
TEST_F(EsimQmiImplTest, GetChallengeTest) {
  const Esim::DataBlob info = {0x00, 0x01, 0x02, 0x03};
  EXPECT_CALL(esim_tester_, OnChallengeResult(info, _)).Times(1);
  EXPECT_CALL(esim_tester_, FakeError(_)).Times(0);

  // This should call OnChallengeResult
  esim_->GetChallenge(
      base::Bind(&EsimQmiTester::OnChallengeResult,
                 base::Unretained(&esim_tester_), info),
      base::Bind(&EsimQmiTester::FakeError, base::Unretained(&esim_tester_)));
}

}  // namespace hermes

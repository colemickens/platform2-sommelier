// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation.h"

#include <base/file_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_platform.h"
#include "mock_tpm.h"

using ::testing::NiceMock;

namespace cryptohome {

static const char* kTestPath = "/tmp/attestation_test.epb";

class AttestationTest : public testing::Test {
 public:
  AttestationTest() : attestation_(&tpm_, &platform_) {
    file_util::Delete(FilePath(kTestPath), true);
    attestation_.set_database_path(kTestPath);
  }
  virtual ~AttestationTest() {
    file_util::Delete(FilePath(kTestPath), true);
  }
 protected:
  NiceMock<MockTpm> tpm_;
  NiceMock<MockPlatform> platform_;
  Attestation attestation_;
};

TEST(AttestationTest_, NullTpm) {
  Attestation without_tpm(NULL, NULL);
  without_tpm.PrepareForEnrollment();
  EXPECT_FALSE(without_tpm.IsPreparedForEnrollment());
}

TEST_F(AttestationTest, PrepareForEnrollment) {
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.IsPreparedForEnrollment());
}

}

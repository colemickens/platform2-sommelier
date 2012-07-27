// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remote_attestation.h"

#include <base/file_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_tpm.h"

using ::testing::NiceMock;

namespace cryptohome {

static const char* kTestPath = "/tmp/attestation_test.epb";

class RemoteAttestationTest : public testing::Test {
 public:
  RemoteAttestationTest() : attestation_(&tpm_) {
    file_util::Delete(FilePath(kTestPath), true);
    attestation_.set_database_path(kTestPath);
  }
  virtual ~RemoteAttestationTest() {
    file_util::Delete(FilePath(kTestPath), true);
  }
 protected:
  NiceMock<MockTpm> tpm_;
  RemoteAttestation attestation_;
};

TEST(RemoteAttestationTest_, NullTpm) {
  RemoteAttestation without_tpm(NULL);
  without_tpm.PrepareForEnrollment();
  EXPECT_FALSE(without_tpm.IsPreparedForEnrollment());
}

TEST_F(RemoteAttestationTest, PrepareForEnrollment) {
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.IsPreparedForEnrollment());
}

}


// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_platform.h"
#include "mock_tpm.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StartsWith;

namespace cryptohome {

static const char* kTestPath = "/tmp/attestation_test.epb";

class AttestationTest : public testing::Test {
 public:
  AttestationTest() : attestation_(&tpm_, &platform_) { }
  virtual ~AttestationTest() { }

  void SetUp() {
    attestation_.set_database_path(kTestPath);
    // Fake up a single file by default.
    ON_CALL(platform_, WriteStringToFile(StartsWith(kTestPath), _))
        .WillByDefault(Invoke(this, &AttestationTest::WriteDB));
    ON_CALL(platform_, ReadFileToString(StartsWith(kTestPath), _))
        .WillByDefault(Invoke(this, &AttestationTest::ReadDB));
  }

  virtual bool WriteDB(const std::string& path, const std::string& db) {
    serialized_db.assign(db);
    return true;
  }
  virtual bool ReadDB(const std::string& path, std::string* db) {
    db->assign(serialized_db);
    return true;
  }

  std::string serialized_db;
 protected:
  NiceMock<MockTpm> tpm_;
  NiceMock<MockPlatform> platform_;
  Attestation attestation_;

  chromeos::SecureBlob GetEnrollBlob() {
    AttestationEnrollmentResponse pb;
    pb.set_status(OK);
    pb.set_detail("");
    pb.mutable_encrypted_identity_credential()->set_asym_ca_contents("1234");
    pb.mutable_encrypted_identity_credential()->set_sym_ca_attestation("5678");
    std::string tmp;
    pb.SerializeToString(&tmp);
    return chromeos::SecureBlob(tmp.data(), tmp.length());
  }

  chromeos::SecureBlob GetCertRequestBlob() {
    AttestationCertificateResponse pb;
    pb.set_status(OK);
    pb.set_detail("");
    pb.set_certified_key_credential("1234");
    std::string tmp;
    pb.SerializeToString(&tmp);
    return chromeos::SecureBlob(tmp.data(), tmp.length());
  }
};

TEST(AttestationTest_, NullTpm) {
  Attestation without_tpm(NULL, NULL);
  without_tpm.PrepareForEnrollment();
  EXPECT_FALSE(without_tpm.IsPreparedForEnrollment());
  EXPECT_FALSE(without_tpm.Verify());
}

TEST_F(AttestationTest, PrepareForEnrollment) {
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.IsPreparedForEnrollment());
}

TEST_F(AttestationTest, Enroll) {
  chromeos::SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateEnrollRequest(&blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.IsEnrolled());
  EXPECT_TRUE(attestation_.CreateEnrollRequest(&blob));
  EXPECT_TRUE(attestation_.Enroll(GetEnrollBlob()));
  EXPECT_TRUE(attestation_.IsEnrolled());
}

TEST_F(AttestationTest, CertRequest) {
  chromeos::SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateCertRequest(false, &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.CreateCertRequest(false, &blob));
  EXPECT_TRUE(attestation_.Enroll(GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(false, &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(), &blob));
  EXPECT_EQ(0, memcmp(blob.data(), "1234", 4));
}

}  // namespace cryptohome

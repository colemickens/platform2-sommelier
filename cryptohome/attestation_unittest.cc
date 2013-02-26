// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_keystore.h"
#include "mock_platform.h"
#include "mock_tpm.h"

#include "cryptolib.h"

using std::string;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgumentPointee;
using ::testing::StartsWith;

namespace cryptohome {

static const char* kTestPath = "/tmp/attestation_test.epb";

class AttestationTest : public testing::Test {
 public:
  AttestationTest() : attestation_(&tpm_, &platform_) { }
  virtual ~AttestationTest() { }

  void SetUp() {
    attestation_.set_database_path(kTestPath);
    attestation_.set_user_key_store(&key_store_);
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
  NiceMock<MockKeyStore> key_store_;
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

  chromeos::SecureBlob GetCertRequestBlob(const chromeos::SecureBlob& request) {
    AttestationCertificateRequest request_pb;
    CHECK(request_pb.ParseFromArray(request.const_data(),
                                    request.size()));
    AttestationCertificateResponse pb;
    pb.set_message_id(request_pb.message_id());
    pb.set_status(OK);
    pb.set_detail("");
    pb.set_certified_key_credential("response_cert");
    pb.set_intermediate_ca_cert("response_ca_cert");
    std::string tmp;
    pb.SerializeToString(&tmp);
    return chromeos::SecureBlob(tmp.data(), tmp.length());
  }

  chromeos::SecureBlob GetCertifiedKeyBlob() {
    CertifiedKey pb;
    pb.set_certified_key_credential("stored_cert");
    pb.set_intermediate_ca_cert("stored_ca_cert");
    pb.set_public_key("public_key");
    std::string tmp;
    pb.SerializeToString(&tmp);
    return chromeos::SecureBlob(tmp.data(), tmp.length());
  }

  bool CompareBlob(const chromeos::SecureBlob& blob, const std::string& str) {
    std::string blob_str(reinterpret_cast<const char*>(blob.const_data()),
                         blob.size());
    if (blob_str != str) {
      printf("Comparing: |%s| with |%s|.\n", blob_str.c_str(), str.c_str());
      return false;
    }
    return true;
    //return (blob_str == str);
  }

  string EncodeCertChain(const string& cert1, const string& cert2) {
    string chain = "-----BEGIN CERTIFICATE-----\n";
    chain += CryptoLib::Base64Encode(cert1, true);
    chain += "-----END CERTIFICATE-----\n-----BEGIN CERTIFICATE-----\n";
    chain += CryptoLib::Base64Encode(cert2, true);
    chain += "-----END CERTIFICATE-----";
    return chain;
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
  chromeos::SecureBlob pubkey("generated_pubkey");
  EXPECT_CALL(tpm_, CreateCertifiedKey(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(pubkey), Return(true)));
  chromeos::SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateCertRequest(false, false, &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.CreateCertRequest(false, false, &blob));
  EXPECT_TRUE(attestation_.Enroll(GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(false, false, &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             false,
                                             "test",
                                             &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  EXPECT_TRUE(attestation_.GetCertificateChain(false, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  EXPECT_TRUE(attestation_.GetPublicKey(false, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "generated_pubkey"));
}

TEST_F(AttestationTest, CertRequestStorageFailure) {
  EXPECT_CALL(key_store_, Write("test", _))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(key_store_, Read("test", _))
      .WillOnce(Return(false))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<1>(GetCertifiedKeyBlob()),
          Return(true)));
  chromeos::SecureBlob blob;
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.Enroll(GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(false, false, &blob));
  // Expect storage failure here.
  EXPECT_FALSE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                              true,
                                              "test",
                                              &blob));
  EXPECT_TRUE(attestation_.CreateCertRequest(false, false, &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             true,
                                             "test",
                                             &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  // Expect storage failure here.
  EXPECT_FALSE(attestation_.GetCertificateChain(true, "test", &blob));
  EXPECT_TRUE(attestation_.GetCertificateChain(true, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("stored_cert",
                                                "stored_ca_cert")));
  EXPECT_TRUE(attestation_.GetPublicKey(true, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, "public_key"));
}

}  // namespace cryptohome

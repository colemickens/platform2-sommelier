// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation.h"

#include <string>

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "mock_keystore.h"
#include "mock_platform.h"
#include "mock_tpm.h"

#include "cryptolib.h"

using chromeos::SecureBlob;
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
  AttestationTest() : attestation_(&tpm_, &platform_), rsa_(NULL) { }
  virtual ~AttestationTest() {
    if (rsa_)
      RSA_free(rsa_);
  }

  void SetUp() {
    attestation_.set_database_path(kTestPath);
    attestation_.set_user_key_store(&key_store_);
    // Fake up a single file by default.
    ON_CALL(platform_, WriteStringToFile(StartsWith(kTestPath), _))
        .WillByDefault(Invoke(this, &AttestationTest::WriteDB));
    ON_CALL(platform_, ReadFileToString(StartsWith(kTestPath), _))
        .WillByDefault(Invoke(this, &AttestationTest::ReadDB));
  }

  virtual bool WriteDB(const string& path, const string& db) {
    serialized_db.assign(db);
    return true;
  }
  virtual bool ReadDB(const string& path, string* db) {
    db->assign(serialized_db);
    return true;
  }

  string serialized_db;
 protected:
  NiceMock<MockTpm> tpm_;
  NiceMock<MockPlatform> platform_;
  NiceMock<MockKeyStore> key_store_;
  Attestation attestation_;
  RSA* rsa_;  // Access with rsa().

  RSA* rsa() {
    if (!rsa_) {
      rsa_ = RSA_generate_key(2048, 65537, NULL, NULL);
      CHECK(rsa_);
    }
    return rsa_;
  }

  SecureBlob ConvertStringToBlob(const string& s) {
    return SecureBlob(s.data(), s.length());
  }

  string ConvertBlobToString(const chromeos::Blob& blob) {
    return string(reinterpret_cast<const char*>(&blob.front()), blob.size());
  }

  SecureBlob GetEnrollBlob() {
    AttestationEnrollmentResponse pb;
    pb.set_status(OK);
    pb.set_detail("");
    pb.mutable_encrypted_identity_credential()->set_asym_ca_contents("1234");
    pb.mutable_encrypted_identity_credential()->set_sym_ca_attestation("5678");
    string tmp;
    pb.SerializeToString(&tmp);
    return SecureBlob(tmp.data(), tmp.length());
  }

  SecureBlob GetCertRequestBlob(const SecureBlob& request) {
    AttestationCertificateRequest request_pb;
    CHECK(request_pb.ParseFromArray(request.const_data(),
                                    request.size()));
    AttestationCertificateResponse pb;
    pb.set_message_id(request_pb.message_id());
    pb.set_status(OK);
    pb.set_detail("");
    pb.set_certified_key_credential("response_cert");
    pb.set_intermediate_ca_cert("response_ca_cert");
    string tmp;
    pb.SerializeToString(&tmp);
    return SecureBlob(tmp.data(), tmp.length());
  }

  SecureBlob GetCertifiedKeyBlob() {
    CertifiedKey pb;
    pb.set_certified_key_credential("stored_cert");
    pb.set_intermediate_ca_cert("stored_ca_cert");
    pb.set_public_key(ConvertBlobToString(GetPKCS1PublicKey()));
    string tmp;
    pb.SerializeToString(&tmp);
    return SecureBlob(tmp.data(), tmp.length());
  }

  bool CompareBlob(const SecureBlob& blob, const string& str) {
    string blob_str(reinterpret_cast<const char*>(blob.const_data()),
                         blob.size());
    return (blob_str == str);
  }

  string EncodeCertChain(const string& cert1, const string& cert2) {
    string chain = "-----BEGIN CERTIFICATE-----\n";
    chain += CryptoLib::Base64Encode(cert1, true);
    chain += "-----END CERTIFICATE-----\n-----BEGIN CERTIFICATE-----\n";
    chain += CryptoLib::Base64Encode(cert2, true);
    chain += "-----END CERTIFICATE-----";
    return chain;
  }

  SecureBlob GetPKCS1PublicKey() {
    unsigned char* buffer = NULL;
    int length = i2d_RSAPublicKey(rsa(), &buffer);
    if (length <= 0)
      return SecureBlob();
    SecureBlob tmp(buffer, length);
    OPENSSL_free(buffer);
    return tmp;
  }

  SecureBlob GetX509PublicKey() {
    unsigned char* buffer = NULL;
    int length = i2d_RSA_PUBKEY(rsa(), &buffer);
    if (length <= 0)
      return SecureBlob();
    SecureBlob tmp(buffer, length);
    OPENSSL_free(buffer);
    return tmp;
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
  SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateEnrollRequest(&blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.IsEnrolled());
  EXPECT_TRUE(attestation_.CreateEnrollRequest(&blob));
  EXPECT_TRUE(attestation_.Enroll(GetEnrollBlob()));
  EXPECT_TRUE(attestation_.IsEnrolled());
}

TEST_F(AttestationTest, CertRequest) {
  EXPECT_CALL(tpm_, CreateCertifiedKey(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(GetPKCS1PublicKey()),
                            Return(true)));
  SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateCertRequest(false, false, &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.CreateCertRequest(false, false, &blob));
  EXPECT_TRUE(attestation_.Enroll(GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(false, false, &blob));
  EXPECT_FALSE(attestation_.DoesKeyExist(false, "test"));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             false,
                                             "test",
                                             &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  EXPECT_TRUE(attestation_.DoesKeyExist(false, "test"));
  EXPECT_TRUE(attestation_.GetCertificateChain(false, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  EXPECT_TRUE(attestation_.GetPublicKey(false, "test", &blob));
  EXPECT_TRUE(blob == GetX509PublicKey());
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
  SecureBlob blob;
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
  EXPECT_TRUE(attestation_.DoesKeyExist(true, "test"));
  EXPECT_TRUE(attestation_.GetCertificateChain(true, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("stored_cert",
                                                "stored_ca_cert")));
  EXPECT_TRUE(attestation_.GetPublicKey(true, "test", &blob));
  EXPECT_TRUE(blob == GetX509PublicKey());
}

}  // namespace cryptohome

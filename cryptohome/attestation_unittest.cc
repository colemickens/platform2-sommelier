// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/attestation.h"

#include <string>

#include <base/stl_util.h>
#include <base/strings/string_util.h>
#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include "cryptohome/crypto.h"
#include "cryptohome/cryptolib.h"
#include "cryptohome/mock_install_attributes.h"
#include "cryptohome/mock_keystore.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/mock_tpm.h"
#include "cryptohome/mock_tpm_init.h"

#include "attestation.pb.h"  // NOLINT(build/include)

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
static const char* kTestUser = "test_user";

class AttestationTest : public testing::Test {
 public:
  AttestationTest() : crypto_(&platform_),
                      attestation_(),
                      rsa_(NULL) {
    crypto_.set_tpm(&tpm_);
    crypto_.set_use_tpm(true);
  }
  virtual ~AttestationTest() {
    if (rsa_)
      RSA_free(rsa_);
  }

  virtual void SetUp() {
    attestation_.set_database_path(kTestPath);
    attestation_.set_user_key_store(&key_store_);
    // Fake up a single file by default.
    ON_CALL(platform_, WriteStringToFile(StartsWith(kTestPath), _))
        .WillByDefault(Invoke(this, &AttestationTest::WriteDB));
    ON_CALL(platform_, ReadFileToString(StartsWith(kTestPath), _))
        .WillByDefault(Invoke(this, &AttestationTest::ReadDB));
    // Configure a TPM that is ready.
    ON_CALL(tpm_, IsEnabled()).WillByDefault(Return(true));
    ON_CALL(tpm_, IsOwned()).WillByDefault(Return(true));
    ON_CALL(tpm_, IsBeingOwned()).WillByDefault(Return(false));
    Initialize();
  }

  virtual void Initialize() {
    attestation_.Initialize(&tpm_, &tpm_init_, &platform_, &crypto_,
                            &install_attributes_);
  }

  virtual bool WriteDB(const string& path, const string& db) {
    serialized_db_.assign(db);
    return true;
  }
  virtual bool ReadDB(const string& path, string* db) {
    if (serialized_db_.empty()) {
      return false;
    }
    db->assign(serialized_db_);
    return true;
  }

 protected:
  string serialized_db_;
  NiceMock<MockTpm> tpm_;
  NiceMock<MockTpmInit> tpm_init_;
  NiceMock<MockPlatform> platform_;
  Crypto crypto_;
  NiceMock<MockKeyStore> key_store_;
  NiceMock<MockInstallAttributes> install_attributes_;
  Attestation attestation_;
  RSA* rsa_;  // Access with rsa().

  RSA* rsa() {
    if (!rsa_) {
      rsa_ = RSA_generate_key(2048, 65537, NULL, NULL);
      CHECK(rsa_);
      attestation_.set_enterprise_test_key(rsa_);
    }
    return rsa_;
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

  SecureBlob GetCertifiedKeyBlob(const string& payload, bool include_ca_cert) {
    CertifiedKey pb;
    pb.set_certified_key_credential("stored_cert");
    if (include_ca_cert)
      pb.set_intermediate_ca_cert("stored_ca_cert");
    pb.set_public_key(GetPKCS1PublicKey().to_string());
    pb.set_payload(payload);
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
    chain += "-----END CERTIFICATE-----";
    if (!cert2.empty()) {
      chain += "\n-----BEGIN CERTIFICATE-----\n";
      chain += CryptoLib::Base64Encode(cert2, true);
      chain += "-----END CERTIFICATE-----";
    }
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

  bool VerifySimpleChallenge(const SecureBlob& response,
                             const string& challenge,
                             const string& signature) {
    SignedData signed_data;
    if (!signed_data.ParseFromArray(response.const_data(), response.size()))
      return false;
    if (signed_data.data().find(challenge) != 0 ||
        signed_data.data() == challenge)
      return false;
    if (signed_data.signature() != signature)
      return false;
    return true;
  }

  bool VerifyEnterpriseChallenge(const SecureBlob& response,
                                 KeyType key_type,
                                 const string& domain,
                                 const string& device_id,
                                 const string& cert_chain,
                                 const string& signature) {
    SignedData signed_data;
    if (!signed_data.ParseFromArray(response.const_data(), response.size()))
      return false;
    ChallengeResponse response_pb;
    if (!response_pb.ParseFromString(signed_data.data()))
      return false;
    string expected_challenge =
        GetEnterpriseChallenge("EnterpriseKeyChallenge", false).to_string();
    if (response_pb.challenge().data() != expected_challenge)
      return false;
    string key_info;
    if (!DecryptData(response_pb.encrypted_key_info(), &key_info))
      return false;
    KeyInfo key_info_pb;
    if (!key_info_pb.ParseFromString(key_info))
      return false;
    if (key_info_pb.domain() != domain ||
        key_info_pb.device_id() != device_id ||
        key_info_pb.key_type() != key_type ||
        key_info_pb.certificate() != cert_chain)
      return false;
    if (signed_data.signature() != signature)
      return false;
    return true;
  }

  SecureBlob GetEnterpriseChallenge(const string& prefix, bool sign) {
    Challenge challenge;
    challenge.set_prefix(prefix);
    challenge.set_nonce("nonce");
    challenge.set_timestamp(123456789);
    string serialized;
    challenge.SerializeToString(&serialized);
    if (sign) {
      unsigned char buffer[256];
      unsigned int length = 0;
      SecureBlob digest = CryptoLib::Sha256(SecureBlob(serialized));
      RSA_sign(NID_sha256,
               &digest.front(), digest.size(),
               buffer, &length,
               rsa());
      SignedData signed_challenge;
      signed_challenge.set_data(serialized);
      signed_challenge.set_signature(buffer, length);
      signed_challenge.SerializeToString(&serialized);
    }
    return SecureBlob(serialized);
  }

  bool DecryptData(const EncryptedData& input, string* output) {
    // Unwrap the AES key.
    unsigned char* wrapped_key_buffer = reinterpret_cast<unsigned char*>(
        const_cast<char*>(input.wrapped_key().data()));
    unsigned char buffer[256];
    int length = RSA_private_decrypt(input.wrapped_key().size(),
                                     wrapped_key_buffer,
                                     buffer,
                                     rsa(),
                                     RSA_PKCS1_OAEP_PADDING);
    if (length != 32)
      return false;
    SecureBlob aes_key(buffer, length);
    // Decrypt the blob.
    SecureBlob decrypted;
    SecureBlob encrypted = SecureBlob(input.encrypted_data());
    SecureBlob aes_iv = SecureBlob(input.iv());
    if (!CryptoLib::AesDecryptSpecifyBlockMode(encrypted, 0, encrypted.size(),
                                               aes_key, aes_iv,
                                               CryptoLib::kPaddingStandard,
                                               CryptoLib::kCbc,
                                               &decrypted))
      return false;
    *output = decrypted.to_string();
    return true;
  }

  // Returns a copy of the database as it exists on 'disk'.
  AttestationDatabase GetPersistentDatabase() {
    AttestationDatabase db;
    attestation_.DecryptDatabase(serialized_db_, &db);
    return db;
  }

  // Returns a mutable reference to the current database instance.  If a test is
  // verifying database changes it should use GetPersistentDatabase() so it will
  // also verify that the changes are written to disk correctly.
  AttestationDatabase& GetMutableDatabase() {
    return attestation_.database_pb_;
  }
};

TEST(AttestationTest_, NullTpm) {
  Crypto crypto(NULL);
  InstallAttributes install_attributes(NULL);
  Attestation without_tpm;
  without_tpm.Initialize(NULL, NULL, NULL, &crypto, &install_attributes);
  without_tpm.PrepareForEnrollment();
  EXPECT_FALSE(without_tpm.IsPreparedForEnrollment());
  EXPECT_FALSE(without_tpm.Verify(false));
  EXPECT_FALSE(without_tpm.VerifyEK(false));
  EXPECT_FALSE(without_tpm.CreateEnrollRequest(Attestation::kDefaultPCA, NULL));
  EXPECT_FALSE(without_tpm.Enroll(Attestation::kDefaultPCA, SecureBlob()));
  EXPECT_FALSE(without_tpm.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", NULL));
  EXPECT_FALSE(without_tpm.FinishCertRequest(SecureBlob(),
                                             false, "", "", NULL));
  EXPECT_FALSE(without_tpm.SignEnterpriseChallenge(false, "", "", "",
                                                   SecureBlob(), false,
                                                   SecureBlob(), NULL));
  EXPECT_FALSE(without_tpm.SignSimpleChallenge(false, "", "", SecureBlob(),
                                               NULL));
  EXPECT_FALSE(without_tpm.GetEKInfo(NULL));
}

TEST_F(AttestationTest, PrepareForEnrollment) {
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.IsPreparedForEnrollment());
  AttestationDatabase db = GetPersistentDatabase();
  EXPECT_TRUE(db.has_credentials());
  EXPECT_TRUE(db.has_identity_binding());
  EXPECT_TRUE(db.has_identity_key());
  EXPECT_TRUE(db.has_pcr0_quote());
  EXPECT_TRUE(db.has_pcr1_quote());
  EXPECT_TRUE(db.has_delegate());
}

TEST_F(AttestationTest, Enroll) {
  SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateEnrollRequest(Attestation::kDefaultPCA,
                                                &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.IsEnrolled());
  EXPECT_TRUE(attestation_.CreateEnrollRequest(Attestation::kDefaultPCA,
                                               &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.IsEnrolled());
}

TEST_F(AttestationTest, CertRequest) {
  EXPECT_CALL(tpm_, CreateCertifiedKey(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(GetPKCS1PublicKey()),
                            Return(true)));
  SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                              ENTERPRISE_USER_CERTIFICATE, "",
                                              "", &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                              ENTERPRISE_USER_CERTIFICATE, "",
                                              "", &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
  EXPECT_FALSE(attestation_.DoesKeyExist(false, kTestUser, "test"));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             false,
                                             kTestUser,
                                             "test",
                                             &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  EXPECT_TRUE(attestation_.DoesKeyExist(false, kTestUser, "test"));
  EXPECT_TRUE(attestation_.GetCertificateChain(false, kTestUser, "test",
                                               &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  EXPECT_TRUE(attestation_.GetPublicKey(false, kTestUser, "test", &blob));
  EXPECT_TRUE(blob == GetX509PublicKey());
}

TEST_F(AttestationTest, CertRequestStorageFailure) {
  EXPECT_CALL(key_store_, Write(kTestUser, "test", _))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(key_store_, Read(kTestUser, "test", _))
      .WillOnce(Return(false))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<2>(GetCertifiedKeyBlob("", true)),
          Return(true)));
  SecureBlob blob;
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
  // Expect storage failure here.
  EXPECT_FALSE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                              true,
                                              kTestUser,
                                              "test",
                                              &blob));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             true,
                                             kTestUser,
                                             "test",
                                             &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("response_cert",
                                                "response_ca_cert")));
  // Expect storage failure here.
  EXPECT_FALSE(attestation_.GetCertificateChain(true, kTestUser, "test",
                                                &blob));
  EXPECT_TRUE(attestation_.DoesKeyExist(true, kTestUser, "test"));
  EXPECT_TRUE(attestation_.GetCertificateChain(true, kTestUser, "test", &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("stored_cert",
                                                "stored_ca_cert")));
  EXPECT_TRUE(attestation_.GetPublicKey(true, kTestUser, "test", &blob));
  EXPECT_TRUE(blob == GetX509PublicKey());
}

TEST_F(AttestationTest, SimpleChallenge) {
  EXPECT_CALL(tpm_, Sign(_, _, _))
      .WillOnce(Return(false))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(SecureBlob("signature")),
                            Return(true)));
  chromeos::SecureBlob blob;
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.CreateEnrollRequest(Attestation::kDefaultPCA,
                                               &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             false,
                                             kTestUser,
                                             "test",
                                             &blob));
  // Expect tpm_.Sign() failure the first attempt.
  EXPECT_FALSE(attestation_.SignSimpleChallenge(false,
                                                kTestUser,
                                                "test",
                                                SecureBlob("challenge"),
                                                &blob));
  EXPECT_TRUE(attestation_.SignSimpleChallenge(false,
                                               kTestUser,
                                               "test",
                                               SecureBlob("challenge"),
                                               &blob));
  EXPECT_TRUE(VerifySimpleChallenge(blob, "challenge", "signature"));
}

TEST_F(AttestationTest, EMKChallenge) {
  EXPECT_CALL(tpm_, Sign(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(SecureBlob("signature")),
                            Return(true)));
  chromeos::SecureBlob blob;
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.CreateEnrollRequest(Attestation::kDefaultPCA,
                                               &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             false,
                                             kTestUser,
                                             "test",
                                             &blob));
  SecureBlob bad_prefix_challenge = GetEnterpriseChallenge("bad", true);
  EXPECT_FALSE(attestation_.SignEnterpriseChallenge(false,
                                                    kTestUser,
                                                    "test",
                                                    "test_domain",
                                                    SecureBlob("test_id"),
                                                    false,
                                                    bad_prefix_challenge,
                                                    &blob));
  SecureBlob challenge = GetEnterpriseChallenge("EnterpriseKeyChallenge", true);
  EXPECT_TRUE(attestation_.SignEnterpriseChallenge(false,
                                                   kTestUser,
                                                   "test",
                                                   "test_domain",
                                                   SecureBlob("test_id"),
                                                   false,
                                                   challenge,
                                                   &blob));
  EXPECT_TRUE(VerifyEnterpriseChallenge(blob,
                                        EMK,
                                        "test_domain",
                                        "test_id",
                                        "",
                                        "signature"));
}

TEST_F(AttestationTest, EUKChallenge) {
  EXPECT_CALL(tpm_, Sign(_, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<2>(SecureBlob("signature")),
                            Return(true)));
  EXPECT_CALL(key_store_, Read(kTestUser, "test", _))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<2>(GetCertifiedKeyBlob("", true)),
          Return(true)));
  chromeos::SecureBlob blob;
  SecureBlob challenge = GetEnterpriseChallenge("EnterpriseKeyChallenge", true);
  EXPECT_TRUE(attestation_.SignEnterpriseChallenge(true,
                                                   kTestUser,
                                                   "test",
                                                   "test_domain",
                                                   SecureBlob("test_id"),
                                                   true,
                                                   challenge,
                                                   &blob));
  EXPECT_TRUE(VerifyEnterpriseChallenge(blob,
                                        EUK,
                                        "test_domain",
                                        "test_id",
                                        EncodeCertChain("stored_cert",
                                                        "stored_ca_cert"),
                                        "signature"));
}

TEST_F(AttestationTest, Payload) {
  EXPECT_CALL(key_store_, Write(kTestUser, "test",
                                GetCertifiedKeyBlob("test_payload", true)))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(key_store_, Read(kTestUser, "test", _))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<2>(GetCertifiedKeyBlob("stored_payload", true)),
          Return(true)));
  EXPECT_CALL(tpm_, CreateCertifiedKey(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(GetPKCS1PublicKey()),
                            Return(true)));
  SecureBlob blob;
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kDefaultPCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
  EXPECT_TRUE(attestation_.FinishCertRequest(GetCertRequestBlob(blob),
                                             false,
                                             kTestUser,
                                             "test",
                                             &blob));
  attestation_.GetKeyPayload(false, kTestUser, "test", &blob);
  EXPECT_EQ(0, blob.size());
  attestation_.SetKeyPayload(false, kTestUser, "test",
                             SecureBlob("test_payload"));
  attestation_.GetKeyPayload(false, kTestUser, "test", &blob);
  EXPECT_TRUE(CompareBlob(blob, "test_payload"));

  attestation_.SetKeyPayload(true, kTestUser, "test",
                             SecureBlob("test_payload"));
  attestation_.GetKeyPayload(true, kTestUser, "test", &blob);
  EXPECT_TRUE(CompareBlob(blob, "stored_payload"));
}

// Tests DeleteKeysByPrefix with device-wide keys stored in the attestation db.
TEST_F(AttestationTest, DeleteByPrefixDevice) {
  // Test with an empty db.
  ASSERT_TRUE(attestation_.DeleteKeysByPrefix(false, "", "prefix"));

  // Test with a single matching key.
  AttestationDatabase& db = GetMutableDatabase();
  db.add_device_keys()->set_key_name("prefix1");
  ASSERT_TRUE(attestation_.DeleteKeysByPrefix(false, "", "prefix"));
  EXPECT_EQ(0, db.device_keys_size());

  // Test with a single non-matching key.
  db.add_device_keys()->set_key_name("other");
  ASSERT_TRUE(attestation_.DeleteKeysByPrefix(false, "", "prefix"));
  EXPECT_EQ(1, db.device_keys_size());

  // Test with an empty prefix.
  ASSERT_TRUE(attestation_.DeleteKeysByPrefix(false, "", ""));
  EXPECT_EQ(0, db.device_keys_size());

  // Test with multiple matching / non-matching keys.
  db.add_device_keys()->set_key_name("prefix1");
  db.add_device_keys()->set_key_name("other1");
  db.add_device_keys()->set_key_name("prefix2");
  db.add_device_keys()->set_key_name("other2");
  db.add_device_keys()->set_key_name("prefix3");
  db.add_device_keys()->set_key_name("other3");
  db.add_device_keys()->set_key_name("prefix4");

  ASSERT_TRUE(attestation_.DeleteKeysByPrefix(false, "", "prefix"));

  EXPECT_EQ(3, db.device_keys_size());
  for (int i = 0; i < db.device_keys_size(); ++i) {
    EXPECT_EQ(0, db.device_keys(i).key_name().find("other"));
  }
}

// Tests DeleteKeysByPrefix with user-owned keys. This object does not manage
// user-owned keys so the test is trivial.
TEST_F(AttestationTest, DeleteByPrefixUser) {
  EXPECT_TRUE(attestation_.DeleteKeysByPrefix(true, kTestUser, "prefix"));
}

TEST_F(AttestationTest, GetEKInfo) {
  string info;
  EXPECT_TRUE(attestation_.GetEKInfo(&info));
  EXPECT_TRUE(base::IsStringASCII(info));

  // Simulate owner password not available.
  EXPECT_CALL(tpm_, GetEndorsementCredential(_))
      .WillRepeatedly(Return(false));
  info.clear();
  EXPECT_FALSE(attestation_.GetEKInfo(&info));
  EXPECT_EQ(0, info.size());
}

TEST_F(AttestationTest, FinalizeEndorsementData) {
  // Simulate first login.
  attestation_.PrepareForEnrollment();
  // Expect endorsement data to be available.
  AttestationDatabase db = GetPersistentDatabase();
  EXPECT_TRUE(db.has_credentials() &&
              db.credentials().has_endorsement_public_key() &&
              db.credentials().has_endorsement_credential());

  // Simulate second login.
  attestation_.Initialize(&tpm_, &tpm_init_, &platform_, &crypto_,
      &install_attributes_);
  // Expect endorsement data to be no longer available.
  db = GetPersistentDatabase();
  EXPECT_TRUE(db.has_credentials() &&
              !db.credentials().has_endorsement_public_key() &&
              !db.credentials().has_endorsement_credential());
}

TEST_F(AttestationTest, CertChainWithNoIntermediateCA) {
  EXPECT_CALL(key_store_, Read(kTestUser, "test", _))
      .WillRepeatedly(DoAll(
          SetArgumentPointee<2>(GetCertifiedKeyBlob("", false)),
          Return(true)));
  SecureBlob blob;
  EXPECT_TRUE(attestation_.GetCertificateChain(true, kTestUser, "test",
                                               &blob));
  EXPECT_TRUE(CompareBlob(blob, EncodeCertChain("stored_cert", "")));
}

TEST_F(AttestationTest, AlternatePCADisabled) {
  // Prepare without alternate PCA configuration.
  attestation_.PrepareForEnrollment();
  // Expect no alternate PCA data to be available.
  AttestationDatabase db = GetPersistentDatabase();
  EXPECT_FALSE(db.has_alternate_identity_binding() ||
               db.has_alternate_identity_key() ||
               db.has_alternate_pcr0_quote() ||
               db.credentials().
                  has_alternate_encrypted_endorsement_credential());
}

TEST_F(AttestationTest, AlternatePCAEnabled) {
  // Prepare with alternate PCA configuration.
  EXPECT_CALL(install_attributes_, Get(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(install_attributes_, Get("enterprise.alternate_pca_key", _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(GetX509PublicKey()),
                            Return(true)));
  EXPECT_CALL(install_attributes_, Get("enterprise.alternate_pca_key", NULL))
      .WillRepeatedly(Return(true));
  attestation_.PrepareForEnrollment();
  // Expect all alternate PCA data to be available.
  AttestationDatabase db = GetPersistentDatabase();
  EXPECT_TRUE(db.has_alternate_identity_binding() &&
              db.has_alternate_identity_key() &&
              db.has_alternate_pcr0_quote() &&
              db.credentials().
                 has_alternate_encrypted_endorsement_credential());
}

TEST_F(AttestationTest, AlternatePCAEnroll) {
  EXPECT_CALL(install_attributes_, Get(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(install_attributes_, Get("enterprise.alternate_pca_key", _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(GetX509PublicKey()),
                            Return(true)));
  EXPECT_CALL(install_attributes_, Get("enterprise.alternate_pca_key", NULL))
      .WillRepeatedly(Return(true));

  SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateEnrollRequest(Attestation::kAlternatePCA,
                                                &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.IsEnrolled());
  EXPECT_TRUE(attestation_.CreateEnrollRequest(Attestation::kDefaultPCA,
                                               &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_FALSE(attestation_.IsEnrolled());
  EXPECT_TRUE(attestation_.CreateEnrollRequest(Attestation::kAlternatePCA,
                                               &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kAlternatePCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.IsEnrolled());
}

TEST_F(AttestationTest, AlternatePCACertRequest) {
  EXPECT_CALL(tpm_, CreateCertifiedKey(_, _, _, _, _, _, _))
      .WillRepeatedly(DoAll(SetArgumentPointee<3>(GetPKCS1PublicKey()),
                            Return(true)));
  EXPECT_CALL(install_attributes_, Get(_, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(install_attributes_, Get("enterprise.alternate_pca_key", _))
      .WillRepeatedly(DoAll(SetArgumentPointee<1>(GetX509PublicKey()),
                            Return(true)));
  EXPECT_CALL(install_attributes_, Get("enterprise.alternate_pca_key", NULL))
      .WillRepeatedly(Return(true));

  SecureBlob blob;
  EXPECT_FALSE(attestation_.CreateCertRequest(Attestation::kAlternatePCA,
                                              ENTERPRISE_USER_CERTIFICATE, "",
                                              "", &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_FALSE(attestation_.CreateCertRequest(Attestation::kAlternatePCA,
                                              ENTERPRISE_USER_CERTIFICATE, "",
                                              "", &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kDefaultPCA, GetEnrollBlob()));
  EXPECT_FALSE(attestation_.CreateCertRequest(Attestation::kAlternatePCA,
                                              ENTERPRISE_USER_CERTIFICATE, "",
                                              "", &blob));
  EXPECT_TRUE(attestation_.Enroll(Attestation::kAlternatePCA, GetEnrollBlob()));
  EXPECT_TRUE(attestation_.CreateCertRequest(Attestation::kAlternatePCA,
                                             ENTERPRISE_USER_CERTIFICATE, "",
                                             "", &blob));
}

TEST_F(AttestationTest, IdentityResetRequest) {
  SecureBlob blob;
  EXPECT_TRUE(attestation_.GetIdentityResetRequest("token", &blob));
  attestation_.PrepareForEnrollment();
  EXPECT_TRUE(attestation_.GetIdentityResetRequest("token", &blob));
}

// An AttestationTest class which does not initialize the Attestation instance.
class AttestationTestNoInitialize : public AttestationTest {
 public:
  virtual void Initialize() {}
};

TEST_F(AttestationTestNoInitialize, AutoExtendPCR1) {
  SecureBlob default_pcr(std::string(20, 0));
  EXPECT_CALL(tpm_, ReadPCR(1, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(default_pcr), Return(true)));
  std::string fake_hwid = "hwid";
  EXPECT_CALL(tpm_, ExtendPCR(1, chromeos::SecureBlob(fake_hwid)))
      .WillOnce(Return(true));
  EXPECT_CALL(platform_, GetHardwareID()).WillRepeatedly(Return(fake_hwid));
  // Now initialize and the mocks will complain if PCR1 is not extended.
  AttestationTest::Initialize();
}

TEST_F(AttestationTestNoInitialize, AutoExtendPCR1NoHwID) {
  SecureBlob default_pcr(std::string(20, 0));
  EXPECT_CALL(tpm_, ReadPCR(1, _))
      .WillOnce(DoAll(SetArgumentPointee<1>(default_pcr), Return(true)));
  std::string no_hwid = "";
  EXPECT_CALL(tpm_, ExtendPCR(_, _)).Times(0);
  EXPECT_CALL(platform_, GetHardwareID()).WillRepeatedly(Return(no_hwid));
  // Now initialize and the mocks will complain if PCR1 is extended.
  AttestationTest::Initialize();
}

}  // namespace cryptohome

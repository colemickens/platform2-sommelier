// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/data_encoding.h>
#include <chromeos/http/http_transport_fake.h>
#include <chromeos/mime_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "attestation/common/attestation_ca.pb.h"
#include "attestation/server/attestation_service.h"
#include "attestation/server/mock_crypto_utility.h"
#include "attestation/server/mock_database.h"
#include "attestation/server/mock_key_store.h"
#include "attestation/server/mock_tpm_utility.h"

using chromeos::http::fake::ServerRequest;
using chromeos::http::fake::ServerResponse;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace attestation {

class AttestationServiceTest : public testing::Test {
 public:
  enum FakeCAState {
    kSuccess,         // Valid successful response.
    kCommandFailure,  // Valid error response.
    kHttpFailure,     // Responds with an HTTP error.
    kBadMessageID,    // Valid successful response but a message ID mismatch.
  };

  ~AttestationServiceTest() override = default;
  void SetUp() override {
    service_.reset(new AttestationService);
    service_->set_database(&mock_database_);
    service_->set_crypto_utility(&mock_crypto_utility_);
    fake_http_transport_ = std::make_shared<chromeos::http::fake::Transport>();
    service_->set_http_transport(fake_http_transport_);
    service_->set_key_store(&mock_key_store_);
    service_->set_tpm_utility(&mock_tpm_utility_);
    // Setup a fake EK certificate by default.
    mock_database_.GetMutableProtobuf()->mutable_credentials()->
        set_endorsement_credential("ek_cert");
    // Setup a fake Attestation CA for success by default.
    SetupFakeCAEnroll(kSuccess);
    SetupFakeCASign(kSuccess);
    CHECK(service_->Initialize());
  }

  void SetupFakeCAEnroll(FakeCAState state) {
    fake_http_transport_->AddHandler(
        service_->attestation_ca_origin() + "/enroll",
        chromeos::http::request_type::kPost,
        base::Bind(&AttestationServiceTest::FakeCAEnroll,
                   base::Unretained(this),
                   state));
  }

  void SetupFakeCASign(FakeCAState state) {
    fake_http_transport_->AddHandler(
        service_->attestation_ca_origin() + "/sign",
        chromeos::http::request_type::kPost,
        base::Bind(&AttestationServiceTest::FakeCASign,
                   base::Unretained(this),
                   state));
  }

  std::string GetFakeCertificateChain() {
    const std::string kBeginCertificate = "-----BEGIN CERTIFICATE-----\n";
    const std::string kEndCertificate = "-----END CERTIFICATE-----";
    std::string pem = kBeginCertificate;
    pem += chromeos::data_encoding::Base64EncodeWrapLines("fake_cert");
    pem += kEndCertificate + "\n" + kBeginCertificate;
    pem += chromeos::data_encoding::Base64EncodeWrapLines("fake_ca_cert");
    pem += kEndCertificate;
    return pem;
  }

  void Run() {
    run_loop_.Run();
  }

  void RunUntilIdle() {
    run_loop_.RunUntilIdle();
  }

  void Quit() {
    run_loop_.Quit();
  }

 protected:
  std::shared_ptr<chromeos::http::fake::Transport> fake_http_transport_;
  NiceMock<MockCryptoUtility> mock_crypto_utility_;
  NiceMock<MockDatabase> mock_database_;
  NiceMock<MockKeyStore> mock_key_store_;
  NiceMock<MockTpmUtility> mock_tpm_utility_;
  std::unique_ptr<AttestationService> service_;

 private:
  void FakeCAEnroll(FakeCAState state,
                    const ServerRequest& request,
                    ServerResponse* response) {
    AttestationEnrollmentRequest request_pb;
    EXPECT_TRUE(request_pb.ParseFromString(request.GetDataAsString()));
    if (state == kHttpFailure) {
      response->ReplyText(chromeos::http::status_code::NotFound, std::string(),
                          chromeos::mime::application::kOctet_stream);
      return;
    }
    AttestationEnrollmentResponse response_pb;
    if (state == kCommandFailure) {
      response_pb.set_status(SERVER_ERROR);
      response_pb.set_detail("fake_enroll_error");
    } else if (state == kSuccess) {
      response_pb.set_status(OK);
      response_pb.set_detail("");
      response_pb.mutable_encrypted_identity_credential()->
          set_asym_ca_contents("1234");
      response_pb.mutable_encrypted_identity_credential()->
          set_sym_ca_attestation("5678");
    } else {
      NOTREACHED();
    }
    std::string tmp;
    response_pb.SerializeToString(&tmp);
    response->ReplyText(chromeos::http::status_code::Ok, tmp,
                        chromeos::mime::application::kOctet_stream);
  }

  void FakeCASign(FakeCAState state,
                  const ServerRequest& request,
                  ServerResponse* response) {
    AttestationCertificateRequest request_pb;
    EXPECT_TRUE(request_pb.ParseFromString(request.GetDataAsString()));
    if (state == kHttpFailure) {
      response->ReplyText(chromeos::http::status_code::NotFound, std::string(),
                          chromeos::mime::application::kOctet_stream);
      return;
    }
    AttestationCertificateResponse response_pb;
    if (state == kCommandFailure) {
      response_pb.set_status(SERVER_ERROR);
      response_pb.set_detail("fake_sign_error");
    } else if (state == kSuccess || state == kBadMessageID) {
      response_pb.set_status(OK);
      response_pb.set_detail("");
      if (state == kSuccess) {
        response_pb.set_message_id(request_pb.message_id());
      }
      response_pb.set_certified_key_credential("fake_cert");
      response_pb.set_intermediate_ca_cert("fake_ca_cert");
    }
    std::string tmp;
    response_pb.SerializeToString(&tmp);
    response->ReplyText(chromeos::http::status_code::Ok, tmp,
                        chromeos::mime::application::kOctet_stream);
  }

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
};

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeySuccess) {
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_EQ(GetFakeCertificateChain(), certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeySuccessNoUser) {
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_SUCCESS, status);
    EXPECT_EQ(GetFakeCertificateChain(), certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithEnrollHttpError) {
  SetupFakeCAEnroll(kHttpFailure);
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_CA_NOT_AVAILABLE, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithSignHttpError) {
  SetupFakeCASign(kHttpFailure);
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_CA_NOT_AVAILABLE, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithCAEnrollFailure) {
  SetupFakeCAEnroll(kCommandFailure);
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_REQUEST_DENIED_BY_CA, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("fake_enroll_error", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithCASignFailure) {
  SetupFakeCASign(kCommandFailure);
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_REQUEST_DENIED_BY_CA, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("fake_sign_error", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithBadCAMessageID) {
  SetupFakeCASign(kBadMessageID);
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_EQ(STATUS_REQUEST_DENIED_BY_CA, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithNoEKCertificate) {
  // Remove the fake EK certificate.
  mock_database_.GetMutableProtobuf()->mutable_credentials()->
      clear_endorsement_credential();
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithRNGFailure) {
  EXPECT_CALL(mock_crypto_utility_, GetRandom(_, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithRNGFailure2) {
  EXPECT_CALL(mock_crypto_utility_, GetRandom(_, _))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithDBFailure) {
  EXPECT_CALL(mock_database_, SaveChanges())
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithDBFailureNoUser) {
  EXPECT_CALL(mock_database_, SaveChanges())
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithKeyReadFailure) {
  EXPECT_CALL(mock_key_store_, Read(_, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithKeyWriteFailure) {
  EXPECT_CALL(mock_key_store_, Write(_, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithTpmNotReady) {
  EXPECT_CALL(mock_tpm_utility_, IsTpmReady())
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithTpmActivateFailure) {
  EXPECT_CALL(mock_tpm_utility_, ActivateIdentity(_, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithTpmCreateFailure) {
  EXPECT_CALL(mock_tpm_utility_, CreateCertifiedKey(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const std::string& certificate_chain,
                         const std::string& server_error,
                         AttestationStatus status) {
    EXPECT_NE(STATUS_SUCCESS, status);
    EXPECT_EQ("", certificate_chain);
    EXPECT_EQ("", server_error);
    Quit();
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyAndCancel) {
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const std::string& certificate_chain,
                                    const std::string& server_error,
                                    AttestationStatus status) {
    callback_count++;
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  // Bring down the service, which should cancel any callbacks.
  service_.reset();
  EXPECT_EQ(0, callback_count);
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyAndCancel2) {
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const std::string& certificate_chain,
                                    const std::string& server_error,
                                    AttestationStatus status) {
    callback_count++;
  };
  service_->CreateGoogleAttestedKey("label", KEY_TYPE_ECC, KEY_USAGE_SIGN,
                                    ENTERPRISE_MACHINE_CERTIFICATE, "user",
                                    "origin", base::Bind(callback));
  // Give threads a chance to run.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
  // Bring down the service, which should cancel any callbacks.
  service_.reset();
  // Pump the loop to make sure no callbacks were posted.
  RunUntilIdle();
  EXPECT_EQ(0, callback_count);
}

}  // namespace attestation

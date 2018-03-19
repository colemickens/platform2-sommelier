//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <brillo/bind_lambda.h>
#include <brillo/data_encoding.h>
#include <brillo/http/http_transport_fake.h>
#include <brillo/mime_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "attestation/common/attestation_ca.pb.h"
#include "attestation/common/mock_crypto_utility.h"
#include "attestation/common/mock_tpm_utility.h"
#include "attestation/server/attestation_service.h"
#include "attestation/server/mock_database.h"
#include "attestation/server/mock_key_store.h"

using brillo::http::fake::ServerRequest;
using brillo::http::fake::ServerResponse;
using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SetArgPointee;

namespace attestation {

// MessageLoopIdleEvent: waits for the moment when the message loop becomes
// idle. Note: it is still possible that there are deferred tasks.
//
// Posts the task to the message loop that checks the following:
// If there are tasks in the incoming queue, the loop is not idle, so re-post
// the task.
// If there are no tasks in the incoming queue, it's still possible that there
// are other tasks in the work queue already picked for processing after this
// task. So, in this case, re-post once again, and check the number of
// tasks between now and the next invocation of this task. If only 1 (this
// task only), the task runner is idle.
class MessageLoopIdleEvent : public base::MessageLoop::TaskObserver {
 public:
  explicit MessageLoopIdleEvent(base::MessageLoop* message_loop)
      : event_(base::WaitableEvent::ResetPolicy::MANUAL,
               base::WaitableEvent::InitialState::NOT_SIGNALED),
        observer_added_(false),
        tasks_processed_(0),
        was_idle_(false),
        message_loop_(message_loop) {
    PostTask();
  }
  ~MessageLoopIdleEvent() {
  }
  // Observer callbacks: WillProcessTask and DidProcessTask.
  // Count the number of run tasks in WillProcessTask.
  void WillProcessTask(const base::PendingTask& pending_task) {
    tasks_processed_++;
  }
  void DidProcessTask(const base::PendingTask& pending_task) { }
  // The task we put on the message loop.
  void RunTask() {
    // We need to add observer in RunTask, since it can only
    // be done by the thread that runs MessageLoop
    if (!observer_added_) {
      message_loop_->AddTaskObserver(this);
      observer_added_ = true;
    }
    bool is_idle = (tasks_processed_ <= 1) &&
                   message_loop_->IsIdleForTesting();
    if (was_idle_ && is_idle) {
      // We need to remove observer in RunTask, since it can only
      // be done by the thread that runs MessageLoop
      if (observer_added_) {
        message_loop_->RemoveTaskObserver(this);
        observer_added_ = false;
      }
      event_.Signal();
      return;
    }
    was_idle_ = is_idle;
    tasks_processed_ = 0;
    PostTask();
  }
  // Waits until the message loop becomes idle.
  void Wait() {
    event_.Wait();
  }

 private:
  void PostTask() {
    auto task = base::Bind(&MessageLoopIdleEvent::RunTask,
                           base::Unretained(this));
    message_loop_->task_runner()->PostTask(FROM_HERE, task);
  }

  // Event to signal when we detect that the message loop is idle.
  base::WaitableEvent event_;
  // Was observer added to the mount loop?
  bool observer_added_;
  // Number of tasks run between previous invocation and now (including this).
  int tasks_processed_;
  // Did the loop appear idle during the previous task invocation?
  bool was_idle_;
  // MessageLoop we are waiting for.
  base::MessageLoop* message_loop_;
};

class AttestationServiceTest : public testing::Test {
 public:
  enum FakeCAState {
    kSuccess,         // Valid successful response.
    kCommandFailure,  // Valid error response.
    kHttpFailure,     // Responds with an HTTP error.
    kBadMessageID,    // Valid successful response but a message ID mismatch.
  };

#ifndef USE_TPM2
  const TpmVersion kTpmVersionUnderTest = TPM_1_2;
#else
  const TpmVersion kTpmVersionUnderTest = TPM_2_0;
#endif

  ~AttestationServiceTest() override = default;

  void SetUp() override {
    service_.reset(new AttestationService(nullptr));
    service_->set_database(&mock_database_);
    service_->set_crypto_utility(&mock_crypto_utility_);
    fake_http_transport_ = std::make_shared<brillo::http::fake::Transport>();
    service_->set_http_transport(fake_http_transport_);
    service_->set_key_store(&mock_key_store_);
    service_->set_tpm_utility(&mock_tpm_utility_);
    service_->set_hwid("fake_hwid");
    // Setup a fake wrapped EK certificate by default.
    mock_database_.GetMutableProtobuf()
        ->mutable_credentials()
        ->mutable_default_encrypted_endorsement_credential()
        ->set_wrapping_key_id("default");
    // Setup a fake Attestation CA for success by default.
    SetupFakeCAEnroll(kSuccess);
    SetupFakeCASign(kSuccess);
    CHECK(service_->Initialize());
    // Run out initialize task(s) to avoid any race conditions with tests that
    // need to change the default setup.
    WaitUntilIdleForTesting();
  }

 protected:
  void SetupFakeCAEnroll(FakeCAState state) {
    fake_http_transport_->AddHandler(
        service_->GetACAWebOrigin(DEFAULT_ACA) + "/enroll",
        brillo::http::request_type::kPost,
        base::Bind(&AttestationServiceTest::FakeCAEnroll,
                   base::Unretained(this), state));
  }

  void SetupFakeCASign(FakeCAState state) {
    fake_http_transport_->AddHandler(
        service_->GetACAWebOrigin(DEFAULT_ACA) + "/sign",
        brillo::http::request_type::kPost,
        base::Bind(&AttestationServiceTest::FakeCASign, base::Unretained(this),
                   state));
  }

  std::string GetFakeCertificateChain() {
    const std::string kBeginCertificate = "-----BEGIN CERTIFICATE-----\n";
    const std::string kEndCertificate = "-----END CERTIFICATE-----";
    std::string pem = kBeginCertificate;
    pem += brillo::data_encoding::Base64EncodeWrapLines("fake_cert");
    pem += kEndCertificate + "\n" + kBeginCertificate;
    pem += brillo::data_encoding::Base64EncodeWrapLines("fake_ca_cert");
    pem += kEndCertificate + "\n" + kBeginCertificate;
    pem += brillo::data_encoding::Base64EncodeWrapLines("fake_ca_cert2");
    pem += kEndCertificate;
    return pem;
  }

  CreateGoogleAttestedKeyRequest GetCreateRequest() {
    CreateGoogleAttestedKeyRequest request;
    request.set_key_label("label");
    request.set_key_type(KEY_TYPE_RSA);
    request.set_key_usage(KEY_USAGE_SIGN);
    request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
    request.set_username("user");
    request.set_origin("origin");
    return request;
  }

  std::string CreateCAEnrollResponse(bool success) {
    AttestationEnrollmentResponse response_pb;
    if (success) {
      response_pb.set_status(OK);
      response_pb.set_detail("");
      response_pb.mutable_encrypted_identity_credential()->set_tpm_version(
          kTpmVersionUnderTest);
      response_pb.mutable_encrypted_identity_credential()->set_asym_ca_contents(
          "1234");
      response_pb.mutable_encrypted_identity_credential()
          ->set_sym_ca_attestation("5678");
      response_pb.mutable_encrypted_identity_credential()->set_encrypted_seed(
          "seed");
      response_pb.mutable_encrypted_identity_credential()->set_credential_mac(
          "mac");
      response_pb.mutable_encrypted_identity_credential()
          ->mutable_wrapped_certificate()->set_wrapped_key("wrapped");
    } else {
      response_pb.set_status(SERVER_ERROR);
      response_pb.set_detail("fake_enroll_error");
    }
    std::string response_str;
    response_pb.SerializeToString(&response_str);
    return response_str;
  }

  std::string CreateCACertResponse(bool success, std::string message_id) {
    AttestationCertificateResponse response_pb;
    if (success) {
      response_pb.set_status(OK);
      response_pb.set_detail("");
      response_pb.set_message_id(message_id);
      response_pb.set_certified_key_credential("fake_cert");
      response_pb.set_intermediate_ca_cert("fake_ca_cert");
      *response_pb.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
    } else {
      response_pb.set_status(SERVER_ERROR);
      response_pb.set_detail("fake_sign_error");
    }
    std::string response_str;
    response_pb.SerializeToString(&response_str);
    return response_str;
  }

  AttestationCertificateRequest GenerateCACertRequest() {
    // Populate identity credential
    AttestationDatabase* database = mock_database_.GetMutableProtobuf();
    database->mutable_identity_key()->set_identity_credential("cert");
    base::RunLoop loop;
    auto callback = [](base::RunLoop* loop,
                       AttestationCertificateRequest* pca_request,
                       const CreateCertificateRequestReply& reply) {
      pca_request->ParseFromString(reply.pca_request());
      loop->Quit();
    };
    CreateCertificateRequestRequest request;
    request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
    request.set_username("user");
    request.set_request_origin("origin");
    AttestationCertificateRequest pca_request;
    service_->CreateCertificateRequest(
        request, base::Bind(callback, &loop, &pca_request));
    loop.Run();
    return pca_request;
  }

  std::string CreateChallenge(const std::string& prefix) {
    Challenge challenge;
    challenge.set_prefix(prefix);
    challenge.set_nonce("nonce");
    challenge.set_timestamp(100500);
    std::string serialized;
    challenge.SerializeToString(&serialized);
    return serialized;
  }

  std::string CreateSignedChallenge(const std::string& prefix) {
    SignedData signed_data;
    signed_data.set_data(CreateChallenge(prefix));
    signed_data.set_signature("challenge_signature");
    std::string serialized;
    signed_data.SerializeToString(&serialized);
    return serialized;
  }

  EncryptedData MockEncryptedData(std::string data) {
    EncryptedData encrypted_data;
    encrypted_data.set_wrapped_key("wrapped_key");
    encrypted_data.set_iv("iv");
    encrypted_data.set_mac("mac");
    encrypted_data.set_encrypted_data(data);
    encrypted_data.set_wrapping_key_id("wrapping_key_id");
    return encrypted_data;
  }

  KeyInfo CreateChallengeKeyInfo() {
    KeyInfo key_info;
    key_info.set_key_type(EUK);
    key_info.set_domain("domain");
    key_info.set_device_id("device_id");
    key_info.set_certificate("");
    return key_info;
  }

  void Run() { run_loop_.Run(); }

  void RunUntilIdle() { run_loop_.RunUntilIdle(); }

  void Quit() { run_loop_.Quit(); }

  void WaitUntilIdleForTesting() {
    MessageLoopIdleEvent idle_event(service_->worker_thread_->message_loop());
    idle_event.Wait();
  }

  std::string ComputeEnterpriseEnrollmentId() {
    return service_->ComputeEnterpriseEnrollmentId();
  }

  std::string GetEnrollmentId() {
    GetEnrollmentIdRequest request;
    auto result = std::make_shared<GetEnrollmentIdReply>();
    service_->GetEnrollmentIdTask(request, result);
    if (result->status() != STATUS_SUCCESS) {
      return "";
    }

    return result->enrollment_id();
  }

  std::shared_ptr<brillo::http::fake::Transport> fake_http_transport_;
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
      response->ReplyText(brillo::http::status_code::NotFound, std::string(),
                          brillo::mime::application::kOctet_stream);
      return;
    }
    std::string response_str;
    if (state == kCommandFailure) {
      response_str = CreateCAEnrollResponse(false);
    } else if (state == kSuccess) {
      response_str = CreateCAEnrollResponse(true);
    } else {
      NOTREACHED();
    }
    response->ReplyText(brillo::http::status_code::Ok, response_str,
                        brillo::mime::application::kOctet_stream);
  }

  void FakeCASign(FakeCAState state,
                  const ServerRequest& request,
                  ServerResponse* response) {
    AttestationCertificateRequest request_pb;
    EXPECT_TRUE(request_pb.ParseFromString(request.GetDataAsString()));
    if (state == kHttpFailure) {
      response->ReplyText(brillo::http::status_code::NotFound, std::string(),
                          brillo::mime::application::kOctet_stream);
      return;
    }
    std::string response_str;
    if (state == kCommandFailure) {
      response_str = CreateCACertResponse(false, "");
    } else if (state == kSuccess) {
      response_str = CreateCACertResponse(true, request_pb.message_id());
    } else if (state == kBadMessageID) {
      response_str = CreateCACertResponse(true, "");
    }
    response->ReplyText(brillo::http::status_code::Ok, response_str,
                        brillo::mime::application::kOctet_stream);
  }

  base::MessageLoop message_loop_;
  base::RunLoop run_loop_;
};

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeySuccess) {
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(GetFakeCertificateChain(), reply.certificate_chain());
    EXPECT_FALSE(reply.has_server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeySuccessNoUser) {
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(GetFakeCertificateChain(), reply.certificate_chain());
    EXPECT_FALSE(reply.has_server_error());
    Quit();
  };
  CreateGoogleAttestedKeyRequest request = GetCreateRequest();
  request.clear_username();
  service_->CreateGoogleAttestedKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithEnrollHttpError) {
  SetupFakeCAEnroll(kHttpFailure);
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_EQ(STATUS_CA_NOT_AVAILABLE, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithSignHttpError) {
  SetupFakeCASign(kHttpFailure);
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_EQ(STATUS_CA_NOT_AVAILABLE, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithCAEnrollFailure) {
  SetupFakeCAEnroll(kCommandFailure);
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_EQ(STATUS_REQUEST_DENIED_BY_CA, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("fake_enroll_error", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithCASignFailure) {
  SetupFakeCASign(kCommandFailure);
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_EQ(STATUS_REQUEST_DENIED_BY_CA, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("fake_sign_error", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithBadCAMessageID) {
  SetupFakeCASign(kBadMessageID);
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithNoEKCertificate) {
  // Remove the default credential setup.
  mock_database_.GetMutableProtobuf()->clear_credentials();
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithRNGFailure) {
  EXPECT_CALL(mock_crypto_utility_, GetRandom(_, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithRNGFailure2) {
  // This flow consumes at least two nonces.
  EXPECT_CALL(mock_crypto_utility_, GetRandom(_, _))
      .WillOnce(Return(true))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithDBFailure) {
  EXPECT_CALL(mock_database_, SaveChanges()).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithDBFailureNoUser) {
  EXPECT_CALL(mock_database_, SaveChanges()).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  CreateGoogleAttestedKeyRequest request = GetCreateRequest();
  request.clear_username();
  service_->CreateGoogleAttestedKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithKeyWriteFailure) {
  EXPECT_CALL(mock_key_store_, Write(_, _, _)).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithTpmNotReady) {
  EXPECT_CALL(mock_tpm_utility_, IsTpmReady()).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithTpmActivateFailure) {
  EXPECT_CALL(mock_tpm_utility_, ActivateIdentity(_, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(mock_tpm_utility_, ActivateIdentityForTpm2(_, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyWithTpmCreateFailure) {
  EXPECT_CALL(mock_tpm_utility_, CreateCertifiedKey(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateGoogleAttestedKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate_chain());
    EXPECT_EQ("", reply.server_error());
    Quit();
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyAndCancel) {
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const CreateGoogleAttestedKeyReply& reply) {
    callback_count++;
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  // Bring down the service, which should cancel any callbacks.
  service_.reset();
  EXPECT_EQ(0, callback_count);
}

TEST_F(AttestationServiceTest, CreateGoogleAttestedKeyAndCancel2) {
  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [&callback_count](const CreateGoogleAttestedKeyReply& reply) {
    callback_count++;
  };
  service_->CreateGoogleAttestedKey(GetCreateRequest(), base::Bind(callback));
  // Give threads a chance to run.
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
  // Bring down the service, which should cancel any callbacks.
  service_.reset();
  // Pump the loop to make sure no callbacks were posted.
  RunUntilIdle();
  EXPECT_EQ(0, callback_count);
}

TEST_F(AttestationServiceTest, GetKeyInfoSuccess) {
  // Setup a certified key in the key store.
  CertifiedKey key;
  key.set_public_key("public_key");
  key.set_certified_key_credential("fake_cert");
  key.set_intermediate_ca_cert("fake_ca_cert");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  key.set_key_name("label");
  key.set_certified_key_info("certify_info");
  key.set_certified_key_proof("signature");
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);
  std::string key_bytes;
  key.SerializeToString(&key_bytes);
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillOnce(DoAll(SetArgPointee<2>(key_bytes), Return(true)));

  // Set expectations on the outputs.
  auto callback = [this](const GetKeyInfoReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(KEY_TYPE_RSA, reply.key_type());
    EXPECT_EQ(KEY_USAGE_SIGN, reply.key_usage());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("certify_info", reply.certify_info());
    EXPECT_EQ("signature", reply.certify_info_signature());
    EXPECT_EQ(GetFakeCertificateChain(), reply.certificate());
    Quit();
  };
  GetKeyInfoRequest request;
  request.set_key_label("label");
  request.set_username("user");
  service_->GetKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetKeyInfoSuccessNoUser) {
  // Setup a certified key in the device key store.
  CertifiedKey& key = *mock_database_.GetMutableProtobuf()->add_device_keys();
  key.set_public_key("public_key");
  key.set_certified_key_credential("fake_cert");
  key.set_intermediate_ca_cert("fake_ca_cert");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  key.set_key_name("label");
  key.set_certified_key_info("certify_info");
  key.set_certified_key_proof("signature");
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);

  // Set expectations on the outputs.
  auto callback = [this](const GetKeyInfoReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(KEY_TYPE_RSA, reply.key_type());
    EXPECT_EQ(KEY_USAGE_SIGN, reply.key_usage());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("certify_info", reply.certify_info());
    EXPECT_EQ("signature", reply.certify_info_signature());
    EXPECT_EQ(GetFakeCertificateChain(), reply.certificate());
    Quit();
  };
  GetKeyInfoRequest request;
  request.set_key_label("label");
  service_->GetKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetKeyInfoNoKey) {
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillRepeatedly(Return(false));

  // Set expectations on the outputs.
  auto callback = [this](const GetKeyInfoReply& reply) {
    EXPECT_EQ(STATUS_INVALID_PARAMETER, reply.status());
    Quit();
  };
  GetKeyInfoRequest request;
  request.set_key_label("label");
  request.set_username("user");
  service_->GetKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetKeyInfoBadPublicKey) {
  EXPECT_CALL(mock_crypto_utility_, GetRSASubjectPublicKeyInfo(_, _))
      .WillRepeatedly(Return(false));

  // Set expectations on the outputs.
  auto callback = [this](const GetKeyInfoReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  GetKeyInfoRequest request;
  request.set_key_label("label");
  request.set_username("user");
  service_->GetKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetEndorsementInfoSuccess) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_credentials()->set_endorsement_public_key("public_key");
  database->mutable_credentials()->set_endorsement_credential("certificate");
  // Set expectations on the outputs.
  auto callback = [this](const GetEndorsementInfoReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.ek_public_key());
    EXPECT_EQ("certificate", reply.ek_certificate());
    Quit();
  };
  GetEndorsementInfoRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  service_->GetEndorsementInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetEndorsementInfoNoInfo) {
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementPublicKey(_, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const GetEndorsementInfoReply& reply) {
    EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
    EXPECT_FALSE(reply.has_ek_public_key());
    EXPECT_FALSE(reply.has_ek_certificate());
    Quit();
  };
  GetEndorsementInfoRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  service_->GetEndorsementInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetEndorsementInfoNoCert) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_credentials()->set_endorsement_public_key("public_key");
  // Set expectations on the outputs.
  auto callback = [this](const GetEndorsementInfoReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.ek_public_key());
    EXPECT_FALSE(reply.has_ek_certificate());
    Quit();
  };
  GetEndorsementInfoRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  service_->GetEndorsementInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetAttestationKeyInfoSuccess) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_identity_key()->set_identity_public_key("public_key");
  database->mutable_identity_key()->set_identity_credential("certificate");
  database->mutable_pcr0_quote()->set_quote("pcr0");
  database->mutable_pcr1_quote()->set_quote("pcr1");
  database->mutable_identity_binding()->set_identity_public_key("public_key2");
  // Set expectations on the outputs.
  auto callback = [this](const GetAttestationKeyInfoReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("public_key2", reply.public_key_tpm_format());
    EXPECT_EQ("certificate", reply.certificate());
    EXPECT_EQ("pcr0", reply.pcr0_quote().quote());
    EXPECT_EQ("pcr1", reply.pcr1_quote().quote());
    Quit();
  };
  GetAttestationKeyInfoRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  service_->GetAttestationKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetAttestationKeyInfoNoInfo) {
  // Set expectations on the outputs.
  auto callback = [this](const GetAttestationKeyInfoReply& reply) {
    EXPECT_EQ(STATUS_NOT_AVAILABLE, reply.status());
    EXPECT_FALSE(reply.has_public_key());
    EXPECT_FALSE(reply.has_public_key_tpm_format());
    EXPECT_FALSE(reply.has_certificate());
    EXPECT_FALSE(reply.has_pcr0_quote());
    EXPECT_FALSE(reply.has_pcr1_quote());
    Quit();
  };
  GetAttestationKeyInfoRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  service_->GetAttestationKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, GetAttestationKeyInfoSomeInfo) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_identity_key()->set_identity_credential("certificate");
  database->mutable_pcr1_quote()->set_quote("pcr1");
  // Set expectations on the outputs.
  auto callback = [this](const GetAttestationKeyInfoReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_public_key());
    EXPECT_FALSE(reply.has_public_key_tpm_format());
    EXPECT_EQ("certificate", reply.certificate());
    EXPECT_FALSE(reply.has_pcr0_quote());
    EXPECT_EQ("pcr1", reply.pcr1_quote().quote());
    Quit();
  };
  GetAttestationKeyInfoRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  service_->GetAttestationKeyInfo(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, ActivateAttestationKeySuccess) {
  EXPECT_CALL(mock_database_, SaveChanges()).Times(1);
  if (kTpmVersionUnderTest == TPM_1_2) {
    EXPECT_CALL(mock_tpm_utility_,
                ActivateIdentity(_, _, _, "encrypted1", "encrypted2", _))
        .WillOnce(DoAll(SetArgPointee<5>(std::string("certificate")),
                        Return(true)));
  } else {
    EXPECT_CALL(
        mock_tpm_utility_,
        ActivateIdentityForTpm2(KEY_TYPE_RSA, _, "seed", "mac", "wrapped", _))
        .WillOnce(DoAll(SetArgPointee<5>(std::string("certificate")),
                        Return(true)));
  }
  // Set expectations on the outputs.
  auto callback = [this](const ActivateAttestationKeyReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("certificate", reply.certificate());
    Quit();
  };
  ActivateAttestationKeyRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  request.mutable_encrypted_certificate()->set_tpm_version(
      kTpmVersionUnderTest);
  request.mutable_encrypted_certificate()->set_asym_ca_contents("encrypted1");
  request.mutable_encrypted_certificate()->set_sym_ca_attestation("encrypted2");
  request.mutable_encrypted_certificate()->set_encrypted_seed("seed");
  request.mutable_encrypted_certificate()->set_credential_mac("mac");
  request.mutable_encrypted_certificate()
      ->mutable_wrapped_certificate()
      ->set_wrapped_key("wrapped");
  request.set_save_certificate(true);
  service_->ActivateAttestationKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, ActivateAttestationKeySuccessNoSave) {
  EXPECT_CALL(mock_database_, GetMutableProtobuf()).Times(0);
  EXPECT_CALL(mock_database_, SaveChanges()).Times(0);
  if (kTpmVersionUnderTest == TPM_1_2) {
    EXPECT_CALL(mock_tpm_utility_,
                ActivateIdentity(_, _, _, "encrypted1", "encrypted2", _))
        .WillOnce(DoAll(SetArgPointee<5>(std::string("certificate")),
                        Return(true)));
  } else {
    EXPECT_CALL(
        mock_tpm_utility_,
        ActivateIdentityForTpm2(KEY_TYPE_RSA, _, "seed", "mac", "wrapped", _))
        .WillOnce(DoAll(SetArgPointee<5>(std::string("certificate")),
                        Return(true)));
  }
  // Set expectations on the outputs.
  auto callback = [this](const ActivateAttestationKeyReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("certificate", reply.certificate());
    Quit();
  };
  ActivateAttestationKeyRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  request.mutable_encrypted_certificate()->set_tpm_version(
      kTpmVersionUnderTest);
  request.mutable_encrypted_certificate()->set_asym_ca_contents("encrypted1");
  request.mutable_encrypted_certificate()->set_sym_ca_attestation("encrypted2");
  request.mutable_encrypted_certificate()->set_encrypted_seed("seed");
  request.mutable_encrypted_certificate()->set_credential_mac("mac");
  request.mutable_encrypted_certificate()
      ->mutable_wrapped_certificate()
      ->set_wrapped_key("wrapped");
  request.set_save_certificate(false);
  service_->ActivateAttestationKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, ActivateAttestationKeySaveFailure) {
  EXPECT_CALL(mock_database_, SaveChanges()).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const ActivateAttestationKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  ActivateAttestationKeyRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  request.mutable_encrypted_certificate()->set_tpm_version(
      kTpmVersionUnderTest);
  request.mutable_encrypted_certificate()->set_asym_ca_contents("encrypted1");
  request.mutable_encrypted_certificate()->set_sym_ca_attestation("encrypted2");
  request.mutable_encrypted_certificate()->set_encrypted_seed("seed");
  request.mutable_encrypted_certificate()->set_credential_mac("mac");
  request.mutable_encrypted_certificate()
      ->mutable_wrapped_certificate()
      ->set_wrapped_key("wrapped");
  request.set_save_certificate(true);
  service_->ActivateAttestationKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, ActivateAttestationKeyActivateFailure) {
  EXPECT_CALL(mock_tpm_utility_,
              ActivateIdentity(_, _, _, "encrypted1", "encrypted2", _))
      .WillRepeatedly(Return(false));
  EXPECT_CALL(
      mock_tpm_utility_,
      ActivateIdentityForTpm2(KEY_TYPE_RSA, _, "seed", "mac", "wrapped", _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const ActivateAttestationKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  ActivateAttestationKeyRequest request;
  request.set_key_type(KEY_TYPE_RSA);
  request.mutable_encrypted_certificate()->set_tpm_version(
      kTpmVersionUnderTest);
  request.mutable_encrypted_certificate()->set_asym_ca_contents("encrypted1");
  request.mutable_encrypted_certificate()->set_sym_ca_attestation("encrypted2");
  request.mutable_encrypted_certificate()->set_encrypted_seed("seed");
  request.mutable_encrypted_certificate()->set_credential_mac("mac");
  request.mutable_encrypted_certificate()
      ->mutable_wrapped_certificate()
      ->set_wrapped_key("wrapped");
  request.set_save_certificate(true);
  service_->ActivateAttestationKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertifiableKeySuccess) {
  // Configure a fake TPM response.
  EXPECT_CALL(
      mock_tpm_utility_,
      CreateCertifiedKey(KEY_TYPE_RSA, KEY_USAGE_SIGN, _, _, _, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<5>(std::string("public_key")),
                SetArgPointee<7>(std::string("certify_info")),
                SetArgPointee<8>(std::string("certify_info_signature")),
                Return(true)));
  // Expect the key to be written exactly once.
  EXPECT_CALL(mock_key_store_, Write("user", "label", _)).Times(1);
  // Set expectations on the outputs.
  auto callback = [this](const CreateCertifiableKeyReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("certify_info", reply.certify_info());
    EXPECT_EQ("certify_info_signature", reply.certify_info_signature());
    Quit();
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_RSA);
  request.set_key_usage(KEY_USAGE_SIGN);
  request.set_username("user");
  service_->CreateCertifiableKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertifiableKeySuccessNoUser) {
  // Configure a fake TPM response.
  EXPECT_CALL(
      mock_tpm_utility_,
      CreateCertifiedKey(KEY_TYPE_RSA, KEY_USAGE_SIGN, _, _, _, _, _, _, _))
      .WillOnce(
          DoAll(SetArgPointee<5>(std::string("public_key")),
                SetArgPointee<7>(std::string("certify_info")),
                SetArgPointee<8>(std::string("certify_info_signature")),
                Return(true)));
  // Expect the key to be written exactly once.
  EXPECT_CALL(mock_database_, SaveChanges()).Times(1);
  // Set expectations on the outputs.
  auto callback = [this](const CreateCertifiableKeyReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("certify_info", reply.certify_info());
    EXPECT_EQ("certify_info_signature", reply.certify_info_signature());
    Quit();
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_RSA);
  request.set_key_usage(KEY_USAGE_SIGN);
  service_->CreateCertifiableKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertifiableKeyRNGFailure) {
  EXPECT_CALL(mock_crypto_utility_, GetRandom(_, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateCertifiableKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_public_key());
    EXPECT_FALSE(reply.has_certify_info());
    EXPECT_FALSE(reply.has_certify_info_signature());
    Quit();
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_RSA);
  request.set_key_usage(KEY_USAGE_SIGN);
  service_->CreateCertifiableKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertifiableKeyTpmCreateFailure) {
  EXPECT_CALL(mock_tpm_utility_, CreateCertifiedKey(_, _, _, _, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateCertifiableKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_public_key());
    EXPECT_FALSE(reply.has_certify_info());
    EXPECT_FALSE(reply.has_certify_info_signature());
    Quit();
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_RSA);
  request.set_key_usage(KEY_USAGE_SIGN);
  service_->CreateCertifiableKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertifiableKeyDBFailure) {
  EXPECT_CALL(mock_key_store_, Write(_, _, _)).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateCertifiableKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_public_key());
    EXPECT_FALSE(reply.has_certify_info());
    EXPECT_FALSE(reply.has_certify_info_signature());
    Quit();
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_RSA);
  request.set_key_usage(KEY_USAGE_SIGN);
  request.set_username("username");
  service_->CreateCertifiableKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertifiableKeyDBFailureNoUser) {
  EXPECT_CALL(mock_database_, SaveChanges()).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const CreateCertifiableKeyReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_public_key());
    EXPECT_FALSE(reply.has_certify_info());
    EXPECT_FALSE(reply.has_certify_info_signature());
    Quit();
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_RSA);
  request.set_key_usage(KEY_USAGE_SIGN);
  service_->CreateCertifiableKey(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, DecryptSuccess) {
  // Set expectations on the outputs.
  auto callback = [this](const DecryptReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(MockTpmUtility::Transform("Unbind", "data"),
              reply.decrypted_data());
    Quit();
  };
  DecryptRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_encrypted_data("data");
  service_->Decrypt(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, DecryptSuccessNoUser) {
  mock_database_.GetMutableProtobuf()->add_device_keys()->set_key_name("label");
  // Set expectations on the outputs.
  auto callback = [this](const DecryptReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(MockTpmUtility::Transform("Unbind", "data"),
              reply.decrypted_data());
    Quit();
  };
  DecryptRequest request;
  request.set_key_label("label");
  request.set_encrypted_data("data");
  service_->Decrypt(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, DecryptKeyNotFound) {
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const DecryptReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_decrypted_data());
    Quit();
  };
  DecryptRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_encrypted_data("data");
  service_->Decrypt(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, DecryptKeyNotFoundNoUser) {
  // Set expectations on the outputs.
  auto callback = [this](const DecryptReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_decrypted_data());
    Quit();
  };
  DecryptRequest request;
  request.set_key_label("label");
  request.set_encrypted_data("data");
  service_->Decrypt(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, DecryptUnbindFailure) {
  EXPECT_CALL(mock_tpm_utility_, Unbind(_, _, _)).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const DecryptReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_decrypted_data());
    Quit();
  };
  DecryptRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_encrypted_data("data");
  service_->Decrypt(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignSuccess) {
  // Set expectations on the outputs.
  auto callback = [this](const SignReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(MockTpmUtility::Transform("Sign", "data"), reply.signature());
    Quit();
  };
  SignRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_data_to_sign("data");
  service_->Sign(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignSuccessNoUser) {
  mock_database_.GetMutableProtobuf()->add_device_keys()->set_key_name("label");
  // Set expectations on the outputs.
  auto callback = [this](const SignReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(MockTpmUtility::Transform("Sign", "data"), reply.signature());
    Quit();
  };
  SignRequest request;
  request.set_key_label("label");
  request.set_data_to_sign("data");
  service_->Sign(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignKeyNotFound) {
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const SignReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_signature());
    Quit();
  };
  SignRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_data_to_sign("data");
  service_->Sign(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignKeyNotFoundNoUser) {
  // Set expectations on the outputs.
  auto callback = [this](const SignReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_signature());
    Quit();
  };
  SignRequest request;
  request.set_key_label("label");
  request.set_data_to_sign("data");
  service_->Sign(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignUnbindFailure) {
  EXPECT_CALL(mock_tpm_utility_, Sign(_, _, _)).WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const SignReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_signature());
    Quit();
  };
  SignRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_data_to_sign("data");
  service_->Sign(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterSuccess) {
  // Setup a key in the user key store.
  CertifiedKey key;
  key.set_key_blob("key_blob");
  key.set_public_key("public_key");
  key.set_certified_key_credential("fake_cert");
  key.set_intermediate_ca_cert("fake_ca_cert");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  key.set_key_name("label");
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);
  std::string key_bytes;
  key.SerializeToString(&key_bytes);
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillOnce(DoAll(SetArgPointee<2>(key_bytes), Return(true)));
  // Cardinality is verified here to verify various steps are performed and to
  // catch performance regressions.
  EXPECT_CALL(mock_key_store_,
              Register("user", "label", KEY_TYPE_RSA, KEY_USAGE_SIGN,
                       "key_blob", "public_key", ""))
      .Times(1);
  EXPECT_CALL(mock_key_store_, RegisterCertificate(_, _))
      .Times(0);
  EXPECT_CALL(mock_key_store_, Delete("user", "label")).Times(1);
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterSuccessNoUser) {
  // Setup a key in the device_keys field.
  CertifiedKey& key = *mock_database_.GetMutableProtobuf()->add_device_keys();
  key.set_key_blob("key_blob");
  key.set_public_key("public_key");
  key.set_certified_key_credential("fake_cert");
  key.set_intermediate_ca_cert("fake_ca_cert");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  key.set_key_name("label");
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);
  // Cardinality is verified here to verify various steps are performed and to
  // catch performance regressions.
  EXPECT_CALL(mock_key_store_,
              Register("", "label", KEY_TYPE_RSA, KEY_USAGE_SIGN, "key_blob",
                       "public_key", ""))
      .Times(1);
  EXPECT_CALL(mock_key_store_, RegisterCertificate(_, _))
      .Times(0);
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(0, mock_database_.GetMutableProtobuf()->device_keys_size());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterSuccessWithCertificates) {
  // Setup a key in the user key store.
  CertifiedKey key;
  key.set_key_blob("key_blob");
  key.set_public_key("public_key");
  key.set_certified_key_credential("fake_cert");
  key.set_intermediate_ca_cert("fake_ca_cert");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  key.set_key_name("label");
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);
  std::string key_bytes;
  key.SerializeToString(&key_bytes);
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillOnce(DoAll(SetArgPointee<2>(key_bytes), Return(true)));
  // Cardinality is verified here to verify various steps are performed and to
  // catch performance regressions.
  EXPECT_CALL(mock_key_store_,
              Register("user", "label", KEY_TYPE_RSA, KEY_USAGE_SIGN,
                       "key_blob", "public_key", "fake_cert"))
      .Times(1);
  EXPECT_CALL(mock_key_store_, RegisterCertificate("user", "fake_ca_cert"))
      .Times(1);
  EXPECT_CALL(mock_key_store_, RegisterCertificate("user", "fake_ca_cert2"))
      .Times(1);
  EXPECT_CALL(mock_key_store_, Delete("user", "label")).Times(1);
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_include_certificates(true);
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterSuccessNoUserWithCertificates) {
  // Setup a key in the device_keys field.
  CertifiedKey& key = *mock_database_.GetMutableProtobuf()->add_device_keys();
  key.set_key_blob("key_blob");
  key.set_public_key("public_key");
  key.set_certified_key_credential("fake_cert");
  key.set_intermediate_ca_cert("fake_ca_cert");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  key.set_key_name("label");
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);
  // Cardinality is verified here to verify various steps are performed and to
  // catch performance regressions.
  EXPECT_CALL(mock_key_store_,
              Register("", "label", KEY_TYPE_RSA, KEY_USAGE_SIGN, "key_blob",
                       "public_key", "fake_cert"))
      .Times(1);
  EXPECT_CALL(mock_key_store_, RegisterCertificate("", "fake_ca_cert"))
      .Times(1);
  EXPECT_CALL(mock_key_store_, RegisterCertificate("", "fake_ca_cert2"))
      .Times(1);
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(0, mock_database_.GetMutableProtobuf()->device_keys_size());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_include_certificates(true);
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterNoKey) {
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterNoKeyNoUser) {
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterFailure) {
  // Setup a key in the user key store.
  CertifiedKey key;
  key.set_key_name("label");
  std::string key_bytes;
  key.SerializeToString(&key_bytes);
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillOnce(DoAll(SetArgPointee<2>(key_bytes), Return(true)));
  EXPECT_CALL(mock_key_store_, Register(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterIntermediateFailure) {
  // Setup a key in the user key store.
  CertifiedKey key;
  key.set_key_name("label");
  key.set_intermediate_ca_cert("fake_ca_cert");
  std::string key_bytes;
  key.SerializeToString(&key_bytes);
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillOnce(DoAll(SetArgPointee<2>(key_bytes), Return(true)));
  EXPECT_CALL(mock_key_store_, RegisterCertificate(_, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_include_certificates(true);
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, RegisterAdditionalFailure) {
  // Setup a key in the user key store.
  CertifiedKey key;
  key.set_key_name("label");
  *key.add_additional_intermediate_ca_cert() = "fake_ca_cert2";
  std::string key_bytes;
  key.SerializeToString(&key_bytes);
  EXPECT_CALL(mock_key_store_, Read("user", "label", _))
      .WillOnce(DoAll(SetArgPointee<2>(key_bytes), Return(true)));
  EXPECT_CALL(mock_key_store_, RegisterCertificate(_, _))
      .WillRepeatedly(Return(false));
  // Set expectations on the outputs.
  auto callback = [this](const RegisterKeyWithChapsTokenReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_include_certificates(true);
  service_->RegisterKeyWithChapsToken(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, PrepareForEnrollment) {
  // Start with an empty database.
  mock_database_.GetMutableProtobuf()->Clear();
  // Schedule initialization again to make sure it runs after this point.
  CHECK(service_->Initialize());
  WaitUntilIdleForTesting();
  EXPECT_TRUE(mock_database_.GetProtobuf().has_credentials());
  EXPECT_TRUE(mock_database_.GetProtobuf().has_identity_key());
  EXPECT_TRUE(mock_database_.GetProtobuf().has_identity_binding());
  EXPECT_TRUE(mock_database_.GetProtobuf().has_pcr0_quote());
  EXPECT_TRUE(mock_database_.GetProtobuf().has_pcr1_quote());
}

TEST_F(AttestationServiceTest, PrepareForEnrollmentNoPublicKey) {
  // Start with an empty database.
  mock_database_.GetMutableProtobuf()->Clear();
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementPublicKey(_, _))
      .WillRepeatedly(Return(false));
  // Schedule initialization again to make sure it runs after this point.
  CHECK(service_->Initialize());
  WaitUntilIdleForTesting();
  EXPECT_FALSE(mock_database_.GetProtobuf().has_credentials());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_key());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_binding());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr0_quote());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr1_quote());
}

TEST_F(AttestationServiceTest, PrepareForEnrollmentNoCert) {
  // Start with an empty database.
  mock_database_.GetMutableProtobuf()->Clear();
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementCertificate(_, _))
      .WillRepeatedly(Return(false));
  // Schedule initialization again to make sure it runs after this point.
  CHECK(service_->Initialize());
  WaitUntilIdleForTesting();
  EXPECT_FALSE(mock_database_.GetProtobuf().has_credentials());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_key());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_binding());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr0_quote());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr1_quote());
}

TEST_F(AttestationServiceTest, PrepareForEnrollmentFailAIK) {
  // Start with an empty database.
  mock_database_.GetMutableProtobuf()->Clear();
  EXPECT_CALL(mock_tpm_utility_, CreateRestrictedKey(_, _, _, _))
      .WillRepeatedly(Return(false));
  // Schedule initialization again to make sure it runs after this point.
  CHECK(service_->Initialize());
  WaitUntilIdleForTesting();
  EXPECT_FALSE(mock_database_.GetProtobuf().has_credentials());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_key());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_binding());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr0_quote());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr1_quote());
}

TEST_F(AttestationServiceTest, PrepareForEnrollmentBadAIK) {
  // Start with an empty database.
  mock_database_.GetMutableProtobuf()->Clear();
  EXPECT_CALL(mock_tpm_utility_, GetRSAPublicKeyFromTpmPublicKey(_, _))
      .WillRepeatedly(Return(false));
  // Schedule initialization again to make sure it runs after this point.
  CHECK(service_->Initialize());
  WaitUntilIdleForTesting();
  EXPECT_FALSE(mock_database_.GetProtobuf().has_credentials());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_key());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_binding());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr0_quote());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr1_quote());
}

TEST_F(AttestationServiceTest, PrepareForEnrollmentFailQuote) {
  // Start with an empty database.
  mock_database_.GetMutableProtobuf()->Clear();
  EXPECT_CALL(mock_tpm_utility_, QuotePCR(_, _, _, _, _))
      .WillRepeatedly(Return(false));
  // Schedule initialization again to make sure it runs after this point.
  CHECK(service_->Initialize());
  WaitUntilIdleForTesting();
  EXPECT_FALSE(mock_database_.GetProtobuf().has_credentials());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_key());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_identity_binding());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr0_quote());
  EXPECT_FALSE(mock_database_.GetProtobuf().has_pcr1_quote());
}

TEST_F(AttestationServiceTest, CreateEnrollRequestSuccessWithoutAbeData) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_credentials()
      ->mutable_default_encrypted_endorsement_credential()
      ->set_wrapped_key("wrapped_key");
  database->mutable_identity_binding()->set_identity_public_key("public_key");
  database->mutable_pcr0_quote()->set_quote("pcr0");
  database->mutable_pcr1_quote()->set_quote("pcr1");
  auto callback = [this](const CreateEnrollRequestReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_pca_request());
    AttestationEnrollmentRequest pca_request;
    EXPECT_TRUE(pca_request.ParseFromString(reply.pca_request()));
    EXPECT_EQ(kTpmVersionUnderTest, pca_request.tpm_version());
    EXPECT_EQ("wrapped_key",
              pca_request.encrypted_endorsement_credential().wrapped_key());
    EXPECT_EQ("public_key", pca_request.identity_public_key());
    EXPECT_EQ("pcr0", pca_request.pcr0_quote().quote());
    EXPECT_EQ("pcr1", pca_request.pcr1_quote().quote());
    EXPECT_FALSE(pca_request.has_enterprise_enrollment_nonce());
    Quit();
  };
  CreateEnrollRequestRequest request;
  service_->CreateEnrollRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateEnrollRequestSuccessWithEmptyAbeData) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_credentials()
      ->mutable_default_encrypted_endorsement_credential()
      ->set_wrapped_key("wrapped_key");
  database->mutable_identity_binding()->set_identity_public_key("public_key");
  database->mutable_pcr0_quote()->set_quote("pcr0");
  database->mutable_pcr1_quote()->set_quote("pcr1");
  auto callback = [this](const CreateEnrollRequestReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_pca_request());
    AttestationEnrollmentRequest pca_request;
    EXPECT_TRUE(pca_request.ParseFromString(reply.pca_request()));
    EXPECT_EQ(kTpmVersionUnderTest, pca_request.tpm_version());
    EXPECT_EQ("wrapped_key",
              pca_request.encrypted_endorsement_credential().wrapped_key());
    EXPECT_EQ("public_key", pca_request.identity_public_key());
    EXPECT_EQ("pcr0", pca_request.pcr0_quote().quote());
    EXPECT_EQ("pcr1", pca_request.pcr1_quote().quote());
    EXPECT_FALSE(pca_request.has_enterprise_enrollment_nonce());
    Quit();
  };
  brillo::SecureBlob abe_data;
  service_->set_abe_data(&abe_data);
  CreateEnrollRequestRequest request;
  service_->CreateEnrollRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateEnrollRequestSuccessWithAbeData) {
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_credentials()
      ->mutable_default_encrypted_endorsement_credential()
      ->set_wrapped_key("wrapped_key");
  database->mutable_identity_binding()->set_identity_public_key("public_key");
  database->mutable_pcr0_quote()->set_quote("pcr0");
  database->mutable_pcr1_quote()->set_quote("pcr1");
  auto callback = [this](const CreateEnrollRequestReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_pca_request());
    AttestationEnrollmentRequest pca_request;
    EXPECT_TRUE(pca_request.ParseFromString(reply.pca_request()));
    EXPECT_EQ(kTpmVersionUnderTest, pca_request.tpm_version());
    EXPECT_EQ("wrapped_key",
              pca_request.encrypted_endorsement_credential().wrapped_key());
    EXPECT_EQ("public_key", pca_request.identity_public_key());
    EXPECT_EQ("pcr0", pca_request.pcr0_quote().quote());
    EXPECT_EQ("pcr1", pca_request.pcr1_quote().quote());
    EXPECT_TRUE(pca_request.has_enterprise_enrollment_nonce());

    // Mocked CryptoUtility->HmacSha256 returns always a zeroed buffer.
    EXPECT_EQ(std::string(32, '\0'), pca_request.enterprise_enrollment_nonce());
    Quit();
  };

  CreateEnrollRequestRequest request;
  brillo::SecureBlob abe_data(0xCA, 32);
  service_->set_abe_data(&abe_data);
  service_->CreateEnrollRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateEnrollRequestNotReady) {
  // Empty database - not prepared for enrollment
  mock_database_.GetMutableProtobuf()->Clear();
  auto callback = [this](const CreateEnrollRequestReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_pca_request());
    Quit();
  };
  CreateEnrollRequestRequest request;
  service_->CreateEnrollRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, FinishEnrollSuccess) {
  auto callback = [this](const FinishEnrollReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    Quit();
  };
  FinishEnrollRequest request;
  request.set_pca_response(CreateCAEnrollResponse(true));
  service_->FinishEnroll(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, FinishEnrollFailure) {
  auto callback = [this](const FinishEnrollReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    Quit();
  };
  FinishEnrollRequest request;
  request.set_pca_response(CreateCAEnrollResponse(false));
  service_->FinishEnroll(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertificateRequestSuccess) {
  // Populate identity credential
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_identity_key()->set_identity_credential("cert");
  auto callback = [this](const CreateCertificateRequestReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_pca_request());
    AttestationCertificateRequest pca_request;
    EXPECT_TRUE(pca_request.ParseFromString(reply.pca_request()));
    EXPECT_EQ(kTpmVersionUnderTest, pca_request.tpm_version());
    EXPECT_EQ(ENTERPRISE_MACHINE_CERTIFICATE, pca_request.profile());
    EXPECT_EQ("cert", pca_request.identity_credential());
    Quit();
  };
  CreateCertificateRequestRequest request;
  request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_username("user");
  request.set_request_origin("origin");
  service_->CreateCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertificateRequestInternalFailure) {
  // Populate identity credential
  AttestationDatabase* database = mock_database_.GetMutableProtobuf();
  database->mutable_identity_key()->set_identity_credential("cert");
  EXPECT_CALL(mock_crypto_utility_, GetRandom(_, _))
      .WillRepeatedly(Return(false));
  auto callback = [this](const CreateCertificateRequestReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_pca_request());
    Quit();
  };
  CreateCertificateRequestRequest request;
  request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_username("user");
  request.set_request_origin("origin");
  service_->CreateCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, CreateCertificateRequestNotEnrolled) {
  // Empty database - not enrolled
  mock_database_.GetMutableProtobuf()->Clear();
  auto callback = [this](const CreateCertificateRequestReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_pca_request());
    Quit();
  };
  CreateCertificateRequestRequest request;
  request.set_certificate_profile(ENTERPRISE_MACHINE_CERTIFICATE);
  request.set_username("user");
  request.set_request_origin("origin");
  service_->CreateCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, FinishCertificateRequestSuccess) {
  auto callback = [this](const FinishCertificateRequestReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_certificate());
    Quit();
  };
  AttestationCertificateRequest pca_request = GenerateCACertRequest();
  FinishCertificateRequestRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_pca_response(
      CreateCACertResponse(true, pca_request.message_id()));
  service_->FinishCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, FinishCertificateRequestInternalFailure) {
  EXPECT_CALL(mock_key_store_, Write(_, _, _)).WillRepeatedly(Return(false));
  auto callback = [this](const FinishCertificateRequestReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate());
    Quit();
  };
  AttestationCertificateRequest pca_request = GenerateCACertRequest();
  FinishCertificateRequestRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_pca_response(
      CreateCACertResponse(true, pca_request.message_id()));
  service_->FinishCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, FinishCertificateRequestWrongMessageId) {
  auto callback = [this](const FinishCertificateRequestReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate());
    Quit();
  };
  // Generate some request to populate pending_requests, but ignore its fields.
  GenerateCACertRequest();
  FinishCertificateRequestRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_pca_response(CreateCACertResponse(true, "wrong_id"));
  service_->FinishCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, FinishCertificateRequestServerFailure) {
  auto callback = [this](const FinishCertificateRequestReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_certificate());
    Quit();
  };
  // Generate some request to populate pending_requests, but ignore its fields.
  GenerateCACertRequest();
  FinishCertificateRequestRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_pca_response(CreateCACertResponse(false, ""));
  service_->FinishCertificateRequest(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignSimpleChallengeSuccess) {
  EXPECT_CALL(mock_tpm_utility_, Sign(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(std::string("signature")),
                      Return(true)));
  auto callback = [this](const SignSimpleChallengeReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_challenge_response());
    SignedData signed_data;
    EXPECT_TRUE(signed_data.ParseFromString(reply.challenge_response()));
    EXPECT_EQ("signature", signed_data.signature());
    EXPECT_EQ(0, signed_data.data().find("challenge"));
    EXPECT_NE("challenge", signed_data.data());
    Quit();
  };
  SignSimpleChallengeRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_challenge("challenge");
  service_->SignSimpleChallenge(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignSimpleChallengeInternalFailure) {
  EXPECT_CALL(mock_tpm_utility_, Sign(_, _, _))
      .WillRepeatedly(Return(false));
  auto callback = [this](const SignSimpleChallengeReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_challenge_response());
    Quit();
  };
  SignSimpleChallengeRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_challenge("challenge");
  service_->SignSimpleChallenge(request, base::Bind(callback));
  Run();
}

class AttestationServiceEnterpriseTest
  : public AttestationServiceTest,
    public testing::WithParamInterface<VAType> {};

TEST_P(AttestationServiceEnterpriseTest, SignEnterpriseChallengeSuccess) {
  KeyInfo key_info = CreateChallengeKeyInfo();
  std::string key_info_str;
  key_info.SerializeToString(&key_info_str);
  EXPECT_CALL(mock_crypto_utility_, VerifySignatureUsingHexKey(
      service_->GetEnterpriseSigningHexKey(GetParam()), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_crypto_utility_,
              EncryptDataForGoogle(key_info_str,
                  service_->GetEnterpriseEncryptionHexKey(GetParam()), _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(MockEncryptedData(key_info_str)),
                Return(true)));
  EXPECT_CALL(mock_tpm_utility_, Sign(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(std::string("signature")),
                      Return(true)));
  auto callback = [this](const SignEnterpriseChallengeReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_challenge_response());
    SignedData signed_data;
    EXPECT_TRUE(signed_data.ParseFromString(reply.challenge_response()));
    EXPECT_EQ("signature", signed_data.signature());
    ChallengeResponse response_pb;
    EXPECT_TRUE(response_pb.ParseFromString(signed_data.data()));
    EXPECT_EQ(CreateChallenge("EnterpriseKeyChallenge"),
              response_pb.challenge().data());
    KeyInfo key_info = CreateChallengeKeyInfo();
    std::string key_info_str;
    key_info.SerializeToString(&key_info_str);
    EXPECT_EQ(key_info_str,
              response_pb.encrypted_key_info().encrypted_data());
    Quit();
  };
  SignEnterpriseChallengeRequest request;
  request.set_va_type(GetParam());
  request.set_username("user");
  request.set_key_label("label");
  request.set_domain(key_info.domain());
  request.set_device_id(key_info.device_id());
  request.set_include_signed_public_key(false);
  request.set_challenge(CreateSignedChallenge("EnterpriseKeyChallenge"));
  service_->SignEnterpriseChallenge(request, base::Bind(callback));
  Run();
}

INSTANTIATE_TEST_CASE_P(
    VerifiedAccessType,
    AttestationServiceEnterpriseTest,
    ::testing::Values(DEFAULT_VA, TEST_VA));

TEST_F(AttestationServiceTest, SignEnterpriseChallengeSuccess) {
  KeyInfo key_info = CreateChallengeKeyInfo();
  std::string key_info_str;
  key_info.SerializeToString(&key_info_str);
  EXPECT_CALL(mock_crypto_utility_, VerifySignatureUsingHexKey(
      service_->GetEnterpriseSigningHexKey(DEFAULT_VA), _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_crypto_utility_,
              EncryptDataForGoogle(key_info_str,
                  service_->GetEnterpriseEncryptionHexKey(DEFAULT_VA), _, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<3>(MockEncryptedData(key_info_str)),
                Return(true)));
  EXPECT_CALL(mock_tpm_utility_, Sign(_, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<2>(std::string("signature")),
                      Return(true)));
  auto callback = [this](const SignEnterpriseChallengeReply& reply) {
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_TRUE(reply.has_challenge_response());
    SignedData signed_data;
    EXPECT_TRUE(signed_data.ParseFromString(reply.challenge_response()));
    EXPECT_EQ("signature", signed_data.signature());
    ChallengeResponse response_pb;
    EXPECT_TRUE(response_pb.ParseFromString(signed_data.data()));
    EXPECT_EQ(CreateChallenge("EnterpriseKeyChallenge"),
              response_pb.challenge().data());
    KeyInfo key_info = CreateChallengeKeyInfo();
    std::string key_info_str;
    key_info.SerializeToString(&key_info_str);
    EXPECT_EQ(key_info_str,
              response_pb.encrypted_key_info().encrypted_data());
    Quit();
  };
  SignEnterpriseChallengeRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_domain(key_info.domain());
  request.set_device_id(key_info.device_id());
  request.set_include_signed_public_key(false);
  request.set_challenge(CreateSignedChallenge("EnterpriseKeyChallenge"));
  service_->SignEnterpriseChallenge(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignEnterpriseChallengeInternalFailure) {
  KeyInfo key_info = CreateChallengeKeyInfo();
  std::string key_info_str;
  key_info.SerializeToString(&key_info_str);
  EXPECT_CALL(mock_crypto_utility_, VerifySignatureUsingHexKey(_, _, _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_tpm_utility_, Sign(_, _, _))
      .WillRepeatedly(Return(false));
  auto callback = [this](const SignEnterpriseChallengeReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_challenge_response());
    Quit();
  };
  SignEnterpriseChallengeRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_domain(key_info.domain());
  request.set_device_id(key_info.device_id());
  request.set_include_signed_public_key(false);
  request.set_challenge(CreateSignedChallenge("EnterpriseKeyChallenge"));
  service_->SignEnterpriseChallenge(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, SignEnterpriseChallengeBadPrefix) {
  KeyInfo key_info = CreateChallengeKeyInfo();
  std::string key_info_str;
  key_info.SerializeToString(&key_info_str);
  EXPECT_CALL(mock_crypto_utility_, VerifySignatureUsingHexKey(_, _, _))
      .WillRepeatedly(Return(true));
  auto callback = [this](const SignEnterpriseChallengeReply& reply) {
    EXPECT_NE(STATUS_SUCCESS, reply.status());
    EXPECT_FALSE(reply.has_challenge_response());
    Quit();
  };
  SignEnterpriseChallengeRequest request;
  request.set_username("user");
  request.set_key_label("label");
  request.set_domain(key_info.domain());
  request.set_device_id(key_info.device_id());
  request.set_include_signed_public_key(false);
  request.set_challenge(CreateSignedChallenge("bad_prefix"));
  service_->SignEnterpriseChallenge(request, base::Bind(callback));
  Run();
}

TEST_F(AttestationServiceTest, ComputeEnterpriseEnrollmentId) {
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementPublicKeyModulus(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(std::string("ekm")),
                      Return(true)));
  brillo::SecureBlob abe_data(0xCA, 32);
  service_->set_abe_data(&abe_data);
  CryptoUtilityImpl crypto_utility(&mock_tpm_utility_);
  service_->set_crypto_utility(&crypto_utility);
  std::string enrollment_id = ComputeEnterpriseEnrollmentId();
  EXPECT_EQ(
      "635c4526dfa583362273e2987944007b09131cfa0f4e5874e7a76d55d333e3cc",
      base::ToLowerASCII(
          base::HexEncode(enrollment_id.data(), enrollment_id.size())));
}

TEST_F(AttestationServiceTest, GetEnrollmentId) {
  EXPECT_CALL(mock_tpm_utility_, GetEndorsementPublicKeyModulus(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(std::string("ekm")),
                      Return(true)));
  brillo::SecureBlob abe_data(0xCA, 32);
  service_->set_abe_data(&abe_data);
  CryptoUtilityImpl crypto_utility(&mock_tpm_utility_);
  service_->set_crypto_utility(&crypto_utility);
  std::string enrollment_id = GetEnrollmentId();
  EXPECT_EQ(
      "635c4526dfa583362273e2987944007b09131cfa0f4e5874e7a76d55d333e3cc",
      base::ToLowerASCII(
          base::HexEncode(enrollment_id.data(), enrollment_id.size())));

  // Cache the enrollment_id in the AIK.
  AttestationDatabase database_pb;
  EXPECT_CALL(mock_database_, GetProtobuf()).WillOnce(ReturnRef(database_pb));
  auto aik = database_pb.mutable_identity_key();
  aik->set_enrollment_id(enrollment_id);

  // Change abe_data, and yet EID should remains the same.
  brillo::SecureBlob abe_data_new(0x89, 32);
  service_->set_abe_data(&abe_data_new);
  enrollment_id = GetEnrollmentId();
  EXPECT_EQ(
      "635c4526dfa583362273e2987944007b09131cfa0f4e5874e7a76d55d333e3cc",
      base::ToLowerASCII(
          base::HexEncode(enrollment_id.data(), enrollment_id.size())));
}

}  // namespace attestation

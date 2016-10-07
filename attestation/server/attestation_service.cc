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

#include "attestation/server/attestation_service.h"

#include <string>

#include <base/callback.h>
#include <base/sha1.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/bind_lambda.h>
#include <brillo/cryptohome.h>
#include <brillo/data_encoding.h>
#include <brillo/http/http_utils.h>
#include <brillo/mime_utils.h>
#include <crypto/sha2.h>
extern "C" {
#include <vboot/crossystem.h>
}

#include "attestation/common/attestation_ca.pb.h"
#include "attestation/common/database.pb.h"
#include "attestation/common/tpm_utility_factory.h"
#include "attestation/server/database_impl.h"

namespace {

#ifndef USE_TEST_ACA
// Google Attestation Certificate Authority (ACA) production instance.
const char kACAWebOrigin[] = "https://chromeos-ca.gstatic.com";
const char kACAPublicKey[] =
    "A2976637E113CC457013F4334312A416395B08D4B2A9724FC9BAD65D0290F39C"
    "866D1163C2CD6474A24A55403C968CF78FA153C338179407FE568C6E550949B1"
    "B3A80731BA9311EC16F8F66060A2C550914D252DB90B44D19BC6C15E923FFCFB"
    "E8A366038772803EE57C7D7E5B3D5E8090BF0960D4F6A6644CB9A456708508F0"
    "6C19245486C3A49F807AB07C65D5E9954F4F8832BC9F882E9EE1AAA2621B1F43"
    "4083FD98758745CBFFD6F55DA699B2EE983307C14C9990DDFB48897F26DF8FB2"
    "CFFF03E631E62FAE59CBF89525EDACD1F7BBE0BA478B5418E756FF3E14AC9970"
    "D334DB04A1DF267D2343C75E5D282A287060D345981ABDA0B2506AD882579FEF";
const char kACAPublicKeyID[] = "\x00\xc7\x0e\x50\xb1";
#else
// Google Attestation Certificate Authority (ACA) test instance.
const char kACAWebOrigin[] = "https://asbestos-qa.corp.google.com";
const char kACAPublicKey[] =
    "A1D50D088994000492B5F3ED8A9C5FC8772706219F4C063B2F6A8C6B74D3AD6B"
    "212A53D01DABB34A6261288540D420D3BA59ED279D859DE6227A7AB6BD88FADD"
    "FC3078D465F4DF97E03A52A587BD0165AE3B180FE7B255B7BEDC1BE81CB1383F"
    "E9E46F9312B1EF28F4025E7D332E33F4416525FEB8F0FC7B815E8FBB79CDABE6"
    "327B5A155FEF13F559A7086CB8A543D72AD6ECAEE2E704FF28824149D7F4E393"
    "D3C74E721ACA97F7ADBE2CCF7B4BCC165F7380F48065F2C8370F25F066091259"
    "D14EA362BAF236E3CD8771A94BDEDA3900577143A238AB92B6C55F11DEFAFB31"
    "7D1DC5B6AE210C52B008D87F2A7BFF6EB5C4FB32D6ECEC6505796173951A3167";
const char kACAPublicKeyID[] = "\x00\xc2\xb0\x56\x2d";
#endif

const char kEnterpriseSigningPublicKey[] =
    "bf7fefa3a661437b26aed0801db64d7ba8b58875c351d3bdc9f653847d4a67b3"
    "b67479327724d56aa0f71a3f57c2290fdc1ff05df80589715e381dfbbda2c4ac"
    "114c30d0a73c5b7b2e22178d26d8b65860aa8dd65e1b3d61a07c81de87c1e7e4"
    "590145624936a011ece10434c1d5d41f917c3dc4b41dd8392479130c4fd6eafc"
    "3bb4e0dedcc8f6a9c28428bf8fbba8bd6438a325a9d3eabee1e89e838138ad99"
    "69c292c6d9f6f52522333b84ddf9471ffe00f01bf2de5faa1621f967f49e158b"
    "f2b305360f886826cc6fdbef11a12b2d6002d70d8d1e8f40e0901ff94c203cb2"
    "01a36a0bd6e83955f14b494f4f2f17c0c826657b85c25ffb8a73599721fa17ab";

const char kEnterpriseEncryptionPublicKey[] =
    "edba5e723da811e41636f792c7a77aef633fbf39b542aa537c93c93eaba7a3b1"
    "0bc3e484388c13d625ef5573358ec9e7fbeb6baaaa87ca87d93fb61bf5760e29"
    "6813c435763ed2c81f631e26e3ff1a670261cdc3c39a4640b6bbf4ead3d6587b"
    "e43ef7f1f08e7596b628ec0b44c9b7ad71c9ee3a1258852c7a986c7614f0c4ec"
    "f0ce147650a53b6aa9ae107374a2d6d4e7922065f2f6eb537a994372e1936c87"
    "eb08318611d44daf6044f8527687dc7ce5319b51eae6ab12bee6bd16e59c499e"
    "fa53d80232ae886c7ee9ad8bc1cbd6e4ac55cb8fa515671f7e7ad66e98769f52"
    "c3c309f98bf08a3b8fbb0166e97906151b46402217e65c5d01ddac8514340e8b";

// This value is opaque; it is proprietary to the system managing the private
// key.  In this case the value has been supplied by the enterprise server
// maintainers.
const char kEnterpriseEncryptionPublicKeyID[] =
    "\x00\x4a\xe2\xdc\xae";

const size_t kNonceSize = 20;  // As per TPM_NONCE definition.
const int kNumTemporalValues = 5;

const std::string kVerifiedBootMode("\x00\x00\x01", 3);

std::string GetHardwareID() {
  char buffer[VB_MAX_STRING_PROPERTY];
  const char* property =
      VbGetSystemPropertyString("hwid", buffer, arraysize(buffer));
  if (property != nullptr) {
    return std::string(property);
  }
  LOG(WARNING) << "Could not read hwid property.";
  return std::string();
}

}  // namespace

namespace attestation {

const size_t kChallengeSignatureNonceSize = 20; // For all TPMs.

AttestationService::AttestationService()
    : attestation_ca_origin_(kACAWebOrigin), weak_factory_(this) {}

bool AttestationService::Initialize() {
  if (!worker_thread_) {
    worker_thread_.reset(new base::Thread("Attestation Service Worker"));
    worker_thread_->StartWithOptions(
        base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
    LOG(INFO) << "Attestation service started.";
  }
  worker_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&AttestationService::InitializeTask, base::Unretained(this)));
  return true;
}

void AttestationService::InitializeTask() {
  if (!tpm_utility_) {
    default_tpm_utility_.reset(TpmUtilityFactory::New());
    CHECK(default_tpm_utility_->Initialize());
    tpm_utility_ = default_tpm_utility_.get();
  }
  if (!crypto_utility_) {
    default_crypto_utility_.reset(new CryptoUtilityImpl(tpm_utility_));
    crypto_utility_ = default_crypto_utility_.get();
  }
  if (!database_) {
    default_database_.reset(new DatabaseImpl(crypto_utility_));
    default_database_->Initialize();
    database_ = default_database_.get();
  }
  if (!key_store_) {
    pkcs11_token_manager_.reset(new chaps::TokenManagerClient());
    default_key_store_.reset(new Pkcs11KeyStore(pkcs11_token_manager_.get()));
    key_store_ = default_key_store_.get();
  }
  if (hwid_.empty()) {
    hwid_ = GetHardwareID();
  }
  if (!IsPreparedForEnrollment()) {
    worker_thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&AttestationService::PrepareForEnrollment,
                              base::Unretained(this)));
  }
}

void AttestationService::CreateGoogleAttestedKey(
    const CreateGoogleAttestedKeyRequest& request,
    const CreateGoogleAttestedKeyCallback& callback) {
  auto result = std::make_shared<CreateGoogleAttestedKeyReply>();
  base::Closure task =
      base::Bind(&AttestationService::CreateGoogleAttestedKeyTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<CreateGoogleAttestedKeyReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::CreateGoogleAttestedKeyTask(
    const CreateGoogleAttestedKeyRequest& request,
    const std::shared_ptr<CreateGoogleAttestedKeyReply>& result) {
  LOG(INFO) << "Creating attested key: " << request.key_label();
  if (!IsPreparedForEnrollment()) {
    LOG(ERROR) << "Attestation: TPM is not ready.";
    result->set_status(STATUS_NOT_READY);
    return;
  }
  if (!IsEnrolled()) {
    std::string enroll_request;
    if (!CreateEnrollRequestInternal(&enroll_request)) {
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
      return;
    }
    std::string enroll_reply;
    if (!SendACARequestAndBlock(kEnroll, enroll_request, &enroll_reply)) {
      result->set_status(STATUS_CA_NOT_AVAILABLE);
      return;
    }
    std::string server_error;
    if (!FinishEnrollInternal(enroll_reply, &server_error)) {
      if (server_error.empty()) {
        result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
        return;
      }
      result->set_status(STATUS_REQUEST_DENIED_BY_CA);
      result->set_server_error(server_error);
      return;
    }
  }
  CertifiedKey key;
  if (!CreateKey(request.username(), request.key_label(), request.key_type(),
                 request.key_usage(), &key)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  std::string certificate_request;
  std::string message_id;
  if (!CreateCertificateRequestInternal(request.username(), key,
                                        request.certificate_profile(), request.origin(),
                                        &certificate_request, &message_id)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  std::string certificate_reply;
  if (!SendACARequestAndBlock(kGetCertificate, certificate_request,
                              &certificate_reply)) {
    result->set_status(STATUS_CA_NOT_AVAILABLE);
    return;
  }
  std::string certificate_chain;
  std::string server_error;
  if (!FinishCertificateRequestInternal(certificate_reply, request.username(),
                                        request.key_label(), message_id, &key,
                                        &certificate_chain, &server_error)) {
    if (server_error.empty()) {
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
      return;
    }
    result->set_status(STATUS_REQUEST_DENIED_BY_CA);
    result->set_server_error(server_error);
    return;
  }
  result->set_certificate_chain(certificate_chain);
}

void AttestationService::GetKeyInfo(const GetKeyInfoRequest& request,
                                    const GetKeyInfoCallback& callback) {
  auto result = std::make_shared<GetKeyInfoReply>();
  base::Closure task = base::Bind(&AttestationService::GetKeyInfoTask,
                                  base::Unretained(this), request, result);
  base::Closure reply =
      base::Bind(&AttestationService::TaskRelayCallback<GetKeyInfoReply>,
                 GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::GetKeyInfoTask(
    const GetKeyInfoRequest& request,
    const std::shared_ptr<GetKeyInfoReply>& result) {
  CertifiedKey key;
  if (!FindKeyByLabel(request.username(), request.key_label(), &key)) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  std::string public_key_info;
  if (!GetSubjectPublicKeyInfo(key.key_type(), key.public_key(),
                               &public_key_info)) {
    LOG(ERROR) << __func__ << ": Bad public key.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_key_type(key.key_type());
  result->set_key_usage(key.key_usage());
  result->set_public_key(public_key_info);
  result->set_certify_info(key.certified_key_info());
  result->set_certify_info_signature(key.certified_key_proof());
  if (key.has_intermediate_ca_cert()) {
    result->set_certificate(CreatePEMCertificateChain(key));
  } else {
    result->set_certificate(key.certified_key_credential());
  }
}

void AttestationService::GetEndorsementInfo(
    const GetEndorsementInfoRequest& request,
    const GetEndorsementInfoCallback& callback) {
  auto result = std::make_shared<GetEndorsementInfoReply>();
  base::Closure task = base::Bind(&AttestationService::GetEndorsementInfoTask,
                                  base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<GetEndorsementInfoReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::GetEndorsementInfoTask(
    const GetEndorsementInfoRequest& request,
    const std::shared_ptr<GetEndorsementInfoReply>& result) {
  if (request.key_type() != KEY_TYPE_RSA) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  auto database_pb = database_->GetProtobuf();
  if (!database_pb.has_credentials() ||
      !database_pb.credentials().has_endorsement_public_key()) {
    // Try to read the public key directly.
    std::string public_key;
    if (!tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_RSA, &public_key)) {
      result->set_status(STATUS_NOT_AVAILABLE);
      return;
    }
    database_pb.mutable_credentials()->set_endorsement_public_key(public_key);
  }
  std::string public_key_info;
  if (!GetSubjectPublicKeyInfo(
          request.key_type(),
          database_pb.credentials().endorsement_public_key(),
          &public_key_info)) {
    LOG(ERROR) << __func__ << ": Bad public key.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_ek_public_key(public_key_info);
  if (database_pb.credentials().has_endorsement_credential()) {
    result->set_ek_certificate(
        database_pb.credentials().endorsement_credential());
  }
  std::string ek_cert;
  if (database_pb.credentials().has_endorsement_credential()) {
    ek_cert = database_pb.credentials().endorsement_credential();
  } else {
    if (!tpm_utility_->GetEndorsementCertificate(KEY_TYPE_RSA, &ek_cert)) {
      LOG(ERROR) << __func__ << ": Endorsement cert not available.";
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
      return;
    }
  }
  std::string hash = crypto::SHA256HashString(ek_cert);
  result->set_ek_info(base::StringPrintf(
      "EK Certificate:\n%s\nHash:\n%s\n",
      CreatePEMCertificate(ek_cert).c_str(),
      base::HexEncode(hash.data(), hash.size()).c_str()));
}

void AttestationService::GetAttestationKeyInfo(
    const GetAttestationKeyInfoRequest& request,
    const GetAttestationKeyInfoCallback& callback) {
  auto result = std::make_shared<GetAttestationKeyInfoReply>();
  base::Closure task =
      base::Bind(&AttestationService::GetAttestationKeyInfoTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<GetAttestationKeyInfoReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::GetAttestationKeyInfoTask(
    const GetAttestationKeyInfoRequest& request,
    const std::shared_ptr<GetAttestationKeyInfoReply>& result) {
  if (request.key_type() != KEY_TYPE_RSA) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  auto database_pb = database_->GetProtobuf();
  if (!IsPreparedForEnrollment() || !database_pb.has_identity_key()) {
    result->set_status(STATUS_NOT_AVAILABLE);
    return;
  }
  if (database_pb.identity_key().has_identity_public_key()) {
    std::string public_key_info;
    if (!GetSubjectPublicKeyInfo(
            request.key_type(),
            database_pb.identity_key().identity_public_key(),
            &public_key_info)) {
      LOG(ERROR) << __func__ << ": Bad public key.";
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
      return;
    }
    result->set_public_key(public_key_info);
  }
  if (database_pb.has_identity_binding() &&
      database_pb.identity_binding().has_identity_public_key()) {
    result->set_public_key_tpm_format(
        database_pb.identity_binding().identity_public_key());
  }
  if (database_pb.identity_key().has_identity_credential()) {
    result->set_certificate(database_pb.identity_key().identity_credential());
  }
  if (database_pb.has_pcr0_quote()) {
    *result->mutable_pcr0_quote() = database_pb.pcr0_quote();
  }
  if (database_pb.has_pcr1_quote()) {
    *result->mutable_pcr1_quote() = database_pb.pcr1_quote();
  }
}

void AttestationService::ActivateAttestationKey(
    const ActivateAttestationKeyRequest& request,
    const ActivateAttestationKeyCallback& callback) {
  auto result = std::make_shared<ActivateAttestationKeyReply>();
  base::Closure task =
      base::Bind(&AttestationService::ActivateAttestationKeyTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<ActivateAttestationKeyReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::ActivateAttestationKeyTask(
    const ActivateAttestationKeyRequest& request,
    const std::shared_ptr<ActivateAttestationKeyReply>& result) {
  if (request.key_type() != KEY_TYPE_RSA) {
    result->set_status(STATUS_INVALID_PARAMETER);
    LOG(ERROR) << __func__ << ": Only RSA currently supported.";
    return;
  }
  if (request.encrypted_certificate().tpm_version() !=
      tpm_utility_->GetVersion()) {
    result->set_status(STATUS_INVALID_PARAMETER);
    LOG(ERROR) << __func__ << ": TPM version mismatch.";
    return;
  }
  std::string certificate;
  if (!ActivateAttestationKeyInternal(request.encrypted_certificate(),
                                      request.save_certificate(),
                                      &certificate)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_certificate(certificate);
}

void AttestationService::CreateCertifiableKey(
    const CreateCertifiableKeyRequest& request,
    const CreateCertifiableKeyCallback& callback) {
  auto result = std::make_shared<CreateCertifiableKeyReply>();
  base::Closure task = base::Bind(&AttestationService::CreateCertifiableKeyTask,
                                  base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<CreateCertifiableKeyReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::CreateCertifiableKeyTask(
    const CreateCertifiableKeyRequest& request,
    const std::shared_ptr<CreateCertifiableKeyReply>& result) {
  CertifiedKey key;
  if (!CreateKey(request.username(), request.key_label(), request.key_type(),
                 request.key_usage(), &key)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  std::string public_key_info;
  if (!GetSubjectPublicKeyInfo(key.key_type(), key.public_key(),
                               &public_key_info)) {
    LOG(ERROR) << __func__ << ": Bad public key.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_public_key(public_key_info);
  result->set_certify_info(key.certified_key_info());
  result->set_certify_info_signature(key.certified_key_proof());
}

void AttestationService::Decrypt(const DecryptRequest& request,
                                 const DecryptCallback& callback) {
  auto result = std::make_shared<DecryptReply>();
  base::Closure task = base::Bind(&AttestationService::DecryptTask,
                                  base::Unretained(this), request, result);
  base::Closure reply =
      base::Bind(&AttestationService::TaskRelayCallback<DecryptReply>,
                 GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::DecryptTask(
    const DecryptRequest& request,
    const std::shared_ptr<DecryptReply>& result) {
  CertifiedKey key;
  if (!FindKeyByLabel(request.username(), request.key_label(), &key)) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  std::string data;
  if (!tpm_utility_->Unbind(key.key_blob(), request.encrypted_data(), &data)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_decrypted_data(data);
}

void AttestationService::Sign(const SignRequest& request,
                              const SignCallback& callback) {
  auto result = std::make_shared<SignReply>();
  base::Closure task = base::Bind(&AttestationService::SignTask,
                                  base::Unretained(this), request, result);
  base::Closure reply =
      base::Bind(&AttestationService::TaskRelayCallback<SignReply>,
                 GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::SignTask(const SignRequest& request,
                                  const std::shared_ptr<SignReply>& result) {
  CertifiedKey key;
  if (!FindKeyByLabel(request.username(), request.key_label(), &key)) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  std::string signature;
  if (!tpm_utility_->Sign(key.key_blob(), request.data_to_sign(), &signature)) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  result->set_signature(signature);
}

void AttestationService::RegisterKeyWithChapsToken(
    const RegisterKeyWithChapsTokenRequest& request,
    const RegisterKeyWithChapsTokenCallback& callback) {
  auto result = std::make_shared<RegisterKeyWithChapsTokenReply>();
  base::Closure task =
      base::Bind(&AttestationService::RegisterKeyWithChapsTokenTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<RegisterKeyWithChapsTokenReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::RegisterKeyWithChapsTokenTask(
    const RegisterKeyWithChapsTokenRequest& request,
    const std::shared_ptr<RegisterKeyWithChapsTokenReply>& result) {
  CertifiedKey key;
  if (!FindKeyByLabel(request.username(), request.key_label(), &key)) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  if (!key_store_->Register(request.username(), request.key_label(),
                            key.key_type(), key.key_usage(), key.key_blob(),
                            key.public_key(), key.certified_key_credential())) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  if (key.has_intermediate_ca_cert() &&
      !key_store_->RegisterCertificate(request.username(),
                                       key.intermediate_ca_cert())) {
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  for (int i = 0; i < key.additional_intermediate_ca_cert_size(); ++i) {
    if (!key_store_->RegisterCertificate(
            request.username(), key.additional_intermediate_ca_cert(i))) {
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
      return;
    }
  }
  DeleteKey(request.username(), request.key_label());
}

bool AttestationService::IsPreparedForEnrollment() {
  if (!tpm_utility_->IsTpmReady()) {
    return false;
  }
  auto database_pb = database_->GetProtobuf();
  if (!database_pb.has_credentials()) {
    return false;
  }
  return (
      database_pb.credentials().has_endorsement_credential() ||
      database_pb.credentials().has_default_encrypted_endorsement_credential());
}

bool AttestationService::IsEnrolled() {
  auto database_pb = database_->GetProtobuf();
  return database_pb.has_identity_key() &&
         database_pb.identity_key().has_identity_credential();
}

bool AttestationService::CreateEnrollRequestInternal(std::string* enroll_request) {
  if (!IsPreparedForEnrollment()) {
    LOG(ERROR) << __func__ << ": Enrollment is not possible, attestation data "
               << "does not exist.";
    return false;
  }
  auto database_pb = database_->GetProtobuf();
  AttestationEnrollmentRequest request_pb;
  request_pb.set_tpm_version(tpm_utility_->GetVersion());
  *request_pb.mutable_encrypted_endorsement_credential() =
      database_pb.credentials().default_encrypted_endorsement_credential();
  request_pb.set_identity_public_key(
      database_pb.identity_binding().identity_public_key());
  *request_pb.mutable_pcr0_quote() = database_pb.pcr0_quote();
  *request_pb.mutable_pcr1_quote() = database_pb.pcr1_quote();
  if (!request_pb.SerializeToString(enroll_request)) {
    LOG(ERROR) << __func__ << ": Failed to serialize protobuf.";
    return false;
  }
  return true;
}

bool AttestationService::FinishEnrollInternal(const std::string& enroll_response,
                                              std::string* server_error) {
  if (!tpm_utility_->IsTpmReady()) {
    return false;
  }
  AttestationEnrollmentResponse response_pb;
  if (!response_pb.ParseFromString(enroll_response)) {
    LOG(ERROR) << __func__ << ": Failed to parse response from CA.";
    return false;
  }
  if (response_pb.status() != OK) {
    *server_error = response_pb.detail();
    LOG(ERROR) << __func__
               << ": Error received from CA: " << response_pb.detail();
    return false;
  }
  if (response_pb.encrypted_identity_credential().tpm_version() !=
      tpm_utility_->GetVersion()) {
    LOG(ERROR) << __func__ << ": TPM version mismatch.";
    return false;
  }
  if (!ActivateAttestationKeyInternal(
          response_pb.encrypted_identity_credential(),
          true /* save_certificate */, nullptr /* certificate */)) {
    return false;
  }
  LOG(INFO) << "Attestation: Enrollment complete.";
  return true;
}

bool AttestationService::CreateCertificateRequestInternal(
    const std::string& username,
    const CertifiedKey& key,
    CertificateProfile profile,
    const std::string& origin,
    std::string* certificate_request,
    std::string* message_id) {
  if (!tpm_utility_->IsTpmReady()) {
    return false;
  }
  if (!IsEnrolled()) {
    LOG(ERROR) << __func__ << ": Device is not enrolled for attestation.";
    return false;
  }
  AttestationCertificateRequest request_pb;
  if (!crypto_utility_->GetRandom(kNonceSize, message_id)) {
    LOG(ERROR) << __func__ << ": GetRandom(message_id) failed.";
    return false;
  }
  request_pb.set_tpm_version(tpm_utility_->GetVersion());
  request_pb.set_message_id(*message_id);
  auto database_pb = database_->GetProtobuf();
  request_pb.set_identity_credential(
      database_pb.identity_key().identity_credential());
  request_pb.set_profile(profile);
  if (!origin.empty() &&
      (profile == CONTENT_PROTECTION_CERTIFICATE_WITH_STABLE_ID)) {
    request_pb.set_origin(origin);
    request_pb.set_temporal_index(ChooseTemporalIndex(username, origin));
  }
  request_pb.set_certified_public_key(key.public_key_tpm_format());
  request_pb.set_certified_key_info(key.certified_key_info());
  request_pb.set_certified_key_proof(key.certified_key_proof());
  if (!request_pb.SerializeToString(certificate_request)) {
    LOG(ERROR) << __func__ << ": Failed to serialize protobuf.";
    return false;
  }
  return true;
}

bool AttestationService::FinishCertificateRequestInternal(
    const std::string& certificate_response,
    const std::string& username,
    const std::string& key_label,
    const std::string& message_id,
    CertifiedKey* key,
    std::string* certificate_chain,
    std::string* server_error) {
  if (!tpm_utility_->IsTpmReady()) {
    return false;
  }
  AttestationCertificateResponse response_pb;
  if (!response_pb.ParseFromString(certificate_response)) {
    LOG(ERROR) << __func__ << ": Failed to parse response from Attestation CA.";
    return false;
  }
  if (response_pb.status() != OK) {
    *server_error = response_pb.detail();
    LOG(ERROR) << __func__ << ": Error received from Attestation CA: "
               << response_pb.detail();
    return false;
  }
  if (message_id != response_pb.message_id()) {
    LOG(ERROR) << __func__ << ": Message ID mismatch.";
    return false;
  }
  return PopulateAndStoreCertifiedKey(response_pb, username, key_label,
                                      key, certificate_chain);
}

bool AttestationService::PopulateAndStoreCertifiedKey(
    const AttestationCertificateResponse& response_pb,
    const std::string& username,
    const std::string& key_label,
    CertifiedKey* key,
    std::string* certificate_chain) {
  // Finish populating the CertifiedKey protobuf and store it.
  key->set_certified_key_credential(response_pb.certified_key_credential());
  key->set_intermediate_ca_cert(response_pb.intermediate_ca_cert());
  key->mutable_additional_intermediate_ca_cert()->MergeFrom(
      response_pb.additional_intermediate_ca_cert());
  if (!SaveKey(username, key_label, *key)) {
    return false;
  }
  LOG(INFO) << "Attestation: Certified key credential received and stored.";
  *certificate_chain = CreatePEMCertificateChain(*key);
  return true;
}

bool AttestationService::SendACARequestAndBlock(ACARequestType request_type,
                                                const std::string& request,
                                                std::string* reply) {
  std::shared_ptr<brillo::http::Transport> transport = http_transport_;
  if (!transport) {
    transport = brillo::http::Transport::CreateDefault();
  }
  std::unique_ptr<brillo::http::Response> response = PostBinaryAndBlock(
      GetACAURL(request_type), request.data(), request.size(),
      brillo::mime::application::kOctet_stream, {},  // headers
      transport,
      nullptr);  // error
  if (!response || !response->IsSuccessful()) {
    LOG(ERROR) << "HTTP request to Attestation CA failed.";
    return false;
  }
  *reply = response->ExtractDataAsString();
  return true;
}

bool AttestationService::FindKeyByLabel(const std::string& username,
                                        const std::string& key_label,
                                        CertifiedKey* key) {
  if (!username.empty()) {
    std::string key_data;
    if (!key_store_->Read(username, key_label, &key_data)) {
      LOG(INFO) << "Key not found: " << key_label;
      return false;
    }
    if (key && !key->ParseFromString(key_data)) {
      LOG(ERROR) << "Failed to parse key: " << key_label;
      return false;
    }
    return true;
  }
  auto database_pb = database_->GetProtobuf();
  for (int i = 0; i < database_pb.device_keys_size(); ++i) {
    if (database_pb.device_keys(i).key_name() == key_label) {
      *key = database_pb.device_keys(i);
      return true;
    }
  }
  LOG(INFO) << "Key not found: " << key_label;
  return false;
}

bool AttestationService::CreateKey(const std::string& username,
                                   const std::string& key_label,
                                   KeyType key_type,
                                   KeyUsage key_usage,
                                   CertifiedKey* key) {
  std::string nonce;
  if (!crypto_utility_->GetRandom(kNonceSize, &nonce)) {
    LOG(ERROR) << __func__ << ": GetRandom(nonce) failed.";
    return false;
  }
  std::string key_blob;
  std::string public_key;
  std::string public_key_tpm_format;
  std::string key_info;
  std::string proof;
  auto database_pb = database_->GetProtobuf();
  if (!tpm_utility_->CreateCertifiedKey(
          key_type, key_usage, database_pb.identity_key().identity_key_blob(),
          nonce, &key_blob, &public_key, &public_key_tpm_format, &key_info,
          &proof)) {
    return false;
  }
  key->set_key_blob(key_blob);
  key->set_public_key(public_key);
  key->set_key_name(key_label);
  key->set_public_key_tpm_format(public_key_tpm_format);
  key->set_certified_key_info(key_info);
  key->set_certified_key_proof(proof);
  key->set_key_type(key_type);
  key->set_key_usage(key_usage);
  return SaveKey(username, key_label, *key);
}

bool AttestationService::SaveKey(const std::string& username,
                                 const std::string& key_label,
                                 const CertifiedKey& key) {
  if (!username.empty()) {
    std::string key_data;
    if (!key.SerializeToString(&key_data)) {
      LOG(ERROR) << __func__ << ": Failed to serialize protobuf.";
      return false;
    }
    if (!key_store_->Write(username, key_label, key_data)) {
      LOG(ERROR) << __func__ << ": Failed to store certified key for user.";
      return false;
    }
  } else {
    if (!AddDeviceKey(key_label, key)) {
      LOG(ERROR) << __func__ << ": Failed to store certified key for device.";
      return false;
    }
  }
  return true;
}

void AttestationService::DeleteKey(const std::string& username,
                                   const std::string& key_label) {
  if (!username.empty()) {
    key_store_->Delete(username, key_label);
  } else {
    RemoveDeviceKey(key_label);
  }
}

bool AttestationService::AddDeviceKey(const std::string& key_label,
                                      const CertifiedKey& key) {
  // If a key by this name already exists, reuse the field.
  auto* database_pb = database_->GetMutableProtobuf();
  bool found = false;
  for (int i = 0; i < database_pb->device_keys_size(); ++i) {
    if (database_pb->device_keys(i).key_name() == key_label) {
      found = true;
      *database_pb->mutable_device_keys(i) = key;
      break;
    }
  }
  if (!found)
    *database_pb->add_device_keys() = key;
  return database_->SaveChanges();
}

void AttestationService::RemoveDeviceKey(const std::string& key_label) {
  auto* database_pb = database_->GetMutableProtobuf();
  bool found = false;
  for (int i = 0; i < database_pb->device_keys_size(); ++i) {
    if (database_pb->device_keys(i).key_name() == key_label) {
      found = true;
      int last = database_pb->device_keys_size() - 1;
      if (i < last) {
        database_pb->mutable_device_keys()->SwapElements(i, last);
      }
      database_pb->mutable_device_keys()->RemoveLast();
      break;
    }
  }
  if (found) {
    if (!database_->SaveChanges()) {
      LOG(WARNING) << __func__ << ": Failed to persist key deletion.";
    }
  }
}

std::string AttestationService::CreatePEMCertificateChain(
    const CertifiedKey& key) {
  if (key.certified_key_credential().empty()) {
    LOG(WARNING) << "Certificate is empty.";
    return std::string();
  }
  std::string pem = CreatePEMCertificate(key.certified_key_credential());
  if (!key.intermediate_ca_cert().empty()) {
    pem += "\n";
    pem += CreatePEMCertificate(key.intermediate_ca_cert());
  }
  for (int i = 0; i < key.additional_intermediate_ca_cert_size(); ++i) {
    pem += "\n";
    pem += CreatePEMCertificate(key.additional_intermediate_ca_cert(i));
  }
  return pem;
}

std::string AttestationService::CreatePEMCertificate(
    const std::string& certificate) {
  const char kBeginCertificate[] = "-----BEGIN CERTIFICATE-----\n";
  const char kEndCertificate[] = "-----END CERTIFICATE-----";

  std::string pem = kBeginCertificate;
  pem += brillo::data_encoding::Base64EncodeWrapLines(certificate);
  pem += kEndCertificate;
  return pem;
}

int AttestationService::ChooseTemporalIndex(const std::string& user,
                                            const std::string& origin) {
  std::string user_hash = crypto::SHA256HashString(user);
  std::string origin_hash = crypto::SHA256HashString(origin);
  int histogram[kNumTemporalValues] = {};
  auto database_pb = database_->GetProtobuf();
  for (int i = 0; i < database_pb.temporal_index_record_size(); ++i) {
    const AttestationDatabase::TemporalIndexRecord& record =
        database_pb.temporal_index_record(i);
    // Ignore out-of-range index values.
    if (record.temporal_index() < 0 ||
        record.temporal_index() >= kNumTemporalValues)
      continue;
    if (record.origin_hash() == origin_hash) {
      if (record.user_hash() == user_hash) {
        // We've previously chosen this index for this user, reuse it.
        return record.temporal_index();
      } else {
        // We've previously chosen this index for another user.
        ++histogram[record.temporal_index()];
      }
    }
  }
  int least_used_index = 0;
  for (int i = 1; i < kNumTemporalValues; ++i) {
    if (histogram[i] < histogram[least_used_index])
      least_used_index = i;
  }
  if (histogram[least_used_index] > 0) {
    LOG(WARNING) << "Unique origin-specific identifiers have been exhausted.";
  }
  // Record our choice for later reference.
  AttestationDatabase::TemporalIndexRecord* new_record =
      database_pb.add_temporal_index_record();
  new_record->set_origin_hash(origin_hash);
  new_record->set_user_hash(user_hash);
  new_record->set_temporal_index(least_used_index);
  database_->SaveChanges();
  return least_used_index;
}

std::string AttestationService::GetACAURL(ACARequestType request_type) const {
  std::string url = attestation_ca_origin_;
  switch (request_type) {
    case kEnroll:
      url += "/enroll";
      break;
    case kGetCertificate:
      url += "/sign";
      break;
    default:
      NOTREACHED();
  }
  return url;
}

bool AttestationService::GetSubjectPublicKeyInfo(
    KeyType key_type,
    const std::string& public_key,
    std::string* public_key_info) const {
  // Only RSA is supported currently.
  if (key_type != KEY_TYPE_RSA) {
    return false;
  }
  return crypto_utility_->GetRSASubjectPublicKeyInfo(public_key,
                                                     public_key_info);
}

void AttestationService::PrepareForEnrollment() {
  if (IsPreparedForEnrollment()) {
    return;
  }
  if (!tpm_utility_->IsTpmReady()) {
    // Try again later.
    worker_thread_->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&AttestationService::PrepareForEnrollment,
                              base::Unretained(this)),
        base::TimeDelta::FromSeconds(3));
    return;
  }
  base::TimeTicks start = base::TimeTicks::Now();
  LOG(INFO) << "Attestation: Preparing for enrollment...";
  // Gather information about the endorsement key.
  std::string rsa_ek_public_key;
  if (!tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_RSA,
                                             &rsa_ek_public_key)) {
    LOG(ERROR) << "Attestation: Failed to get RSA EK public key.";
    return;
  }
  std::string ecc_ek_public_key;
  if (!tpm_utility_->GetEndorsementPublicKey(KEY_TYPE_ECC,
                                             &ecc_ek_public_key)) {
    LOG(WARNING) << "Attestation: Failed to get ECC EK public key.";
  }
  std::string rsa_ek_certificate;
  if (!tpm_utility_->GetEndorsementCertificate(KEY_TYPE_RSA,
                                               &rsa_ek_certificate)) {
    LOG(ERROR) << "Attestation: Failed to get RSA EK certificate.";
    return;
  }
  std::string ecc_ek_certificate;
  if (!tpm_utility_->GetEndorsementCertificate(KEY_TYPE_ECC,
                                               &ecc_ek_certificate)) {
    LOG(WARNING) << "Attestation: Failed to get ECC EK certificate.";
  }
  // Create an identity key.
  std::string rsa_identity_public_key;
  std::string rsa_identity_key_blob;
  if (!tpm_utility_->CreateRestrictedKey(KEY_TYPE_RSA, KEY_USAGE_SIGN,
                                         &rsa_identity_public_key,
                                         &rsa_identity_key_blob)) {
    LOG(ERROR) << "Attestation: Failed to create RSA AIK.";
    return;
  }
  std::string rsa_identity_public_key_der;
  if (!tpm_utility_->GetRSAPublicKeyFromTpmPublicKey(
          rsa_identity_public_key, &rsa_identity_public_key_der)) {
    LOG(ERROR) << "Attestation: Failed to parse AIK public key.";
    return;
  }
  // Quote PCRs. These quotes are intended to be valid for the lifetime of the
  // identity key so they do not need external data. This only works when
  // firmware ensures that these PCRs will not change unless the TPM owner is
  // cleared.
  std::string quoted_pcr_value0;
  std::string quoted_data0;
  std::string quote0;
  if (!tpm_utility_->QuotePCR(0, rsa_identity_key_blob, &quoted_pcr_value0,
                              &quoted_data0, &quote0)) {
    LOG(ERROR) << "Attestation: Failed to generate quote for PCR_0.";
    return;
  }
  std::string quoted_pcr_value1;
  std::string quoted_data1;
  std::string quote1;
  if (!tpm_utility_->QuotePCR(1, rsa_identity_key_blob, &quoted_pcr_value1,
                              &quoted_data1, &quote1)) {
    LOG(ERROR) << "Attestation: Failed to generate quote for PCR_1.";
    return;
  }
  // Store all this in the attestation database.
  AttestationDatabase* database_pb = database_->GetMutableProtobuf();
  TPMCredentials* credentials_pb = database_pb->mutable_credentials();
  credentials_pb->set_endorsement_public_key(rsa_ek_public_key);
  credentials_pb->set_endorsement_credential(rsa_ek_certificate);
  credentials_pb->set_ecc_endorsement_public_key(ecc_ek_public_key);
  credentials_pb->set_ecc_endorsement_credential(ecc_ek_certificate);

  if (!crypto_utility_->EncryptDataForGoogle(
          rsa_ek_certificate, kACAPublicKey,
          std::string(kACAPublicKeyID, arraysize(kACAPublicKeyID) - 1),
          credentials_pb->mutable_default_encrypted_endorsement_credential())) {
    LOG(ERROR) << "Attestation: Failed to encrypt EK certificate.";
    return;
  }
  IdentityKey* key_pb = database_pb->mutable_identity_key();
  key_pb->set_identity_public_key(rsa_identity_public_key_der);
  key_pb->set_identity_key_blob(rsa_identity_key_blob);
  IdentityBinding* binding_pb = database_pb->mutable_identity_binding();
  binding_pb->set_identity_public_key_der(rsa_identity_public_key_der);
  binding_pb->set_identity_public_key(rsa_identity_public_key);
  Quote* quote_pb0 = database_pb->mutable_pcr0_quote();
  quote_pb0->set_quote(quote0);
  quote_pb0->set_quoted_data(quoted_data0);
  quote_pb0->set_quoted_pcr_value(quoted_pcr_value0);
  Quote* quote_pb1 = database_pb->mutable_pcr1_quote();
  quote_pb1->set_quote(quote1);
  quote_pb1->set_quoted_data(quoted_data1);
  quote_pb1->set_quoted_pcr_value(quoted_pcr_value1);
  quote_pb1->set_pcr_source_hint(hwid_);
  if (!database_->SaveChanges()) {
    LOG(ERROR) << "Attestation: Failed to write database.";
    return;
  }
  base::TimeDelta delta = (base::TimeTicks::Now() - start);
  LOG(INFO) << "Attestation: Prepared successfully (" << delta.InMilliseconds()
            << "ms).";
}

bool AttestationService::ActivateAttestationKeyInternal(
    const EncryptedIdentityCredential& encrypted_certificate,
    bool save_certificate,
    std::string* certificate) {
  std::string certificate_local;
  auto database_pb = database_->GetProtobuf();
  if (encrypted_certificate.tpm_version() == TPM_1_2) {
    // TPM 1.2 style activate.
    if (!tpm_utility_->ActivateIdentity(
            database_pb.delegate().blob(), database_pb.delegate().secret(),
            database_pb.identity_key().identity_key_blob(),
            encrypted_certificate.asym_ca_contents(),
            encrypted_certificate.sym_ca_attestation(),
            &certificate_local)) {
      LOG(ERROR) << __func__ << ": Failed to activate identity.";
      return false;
    }
  } else {
    // TPM 2.0 style activate.
    std::string credential;
    if (!tpm_utility_->ActivateIdentityForTpm2(
            KEY_TYPE_RSA, database_pb.identity_key().identity_key_blob(),
            encrypted_certificate.encrypted_seed(),
            encrypted_certificate.credential_mac(),
            encrypted_certificate.wrapped_certificate().wrapped_key(),
            &credential)) {
      LOG(ERROR) << __func__ << ": Failed to activate identity.";
      return false;
    }
    if (!crypto_utility_->DecryptIdentityCertificateForTpm2(
            credential, encrypted_certificate.wrapped_certificate(),
            &certificate_local)) {
      LOG(ERROR) << __func__ << ": Failed to decrypt identity certificate.";
      return false;
    }
  }
  if (save_certificate) {
    database_->GetMutableProtobuf()
        ->mutable_identity_key()
        ->set_identity_credential(certificate_local);
    if (!database_->SaveChanges()) {
      LOG(ERROR) << __func__ << ": Failed to persist database changes.";
      return false;
    }
  }
  if (certificate) {
    *certificate = certificate_local;
  }
  return true;
}

void AttestationService::GetStatus(
    const GetStatusRequest& request,
    const GetStatusCallback& callback) {
  auto result = std::make_shared<GetStatusReply>();
  base::Closure task =
      base::Bind(&AttestationService::GetStatusTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<GetStatusReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

bool AttestationService::IsVerifiedMode() const {
  if (!tpm_utility_->IsTpmReady()) {
    VLOG(2) << __func__ << ": Tpm is not ready.";
    return false;
  }
  std::string current_pcr_value;
  if (!tpm_utility_->ReadPCR(0, &current_pcr_value)) {
    LOG(WARNING) << __func__ << ": Failed to read PCR0.";
    return false;
  }
  std::string verified_mode = base::SHA1HashString(kVerifiedBootMode);
  std::string expected_pcr_value;
  if (tpm_utility_->GetVersion() == TPM_1_2) {
    // Use SHA-1 digests for TPM 1.2.
    std::string initial(base::kSHA1Length, 0);
    expected_pcr_value = base::SHA1HashString(initial + verified_mode);
  } else if (tpm_utility_->GetVersion() == TPM_2_0) {
    // Use SHA-256 digests for TPM 2.0.
    std::string initial(crypto::kSHA256Length, 0);
    verified_mode.resize(crypto::kSHA256Length);
    expected_pcr_value = crypto::SHA256HashString(initial + verified_mode);
  } else {
    LOG(ERROR) << __func__ << ": Unsupported TPM version.";
    return false;
  }
  return (current_pcr_value == expected_pcr_value);
}

void AttestationService::GetStatusTask(
    const GetStatusRequest& request,
    const std::shared_ptr<GetStatusReply>& result) {
  result->set_prepared_for_enrollment(IsPreparedForEnrollment());
  result->set_enrolled(IsEnrolled());
  if (request.extended_status()) {
    result->set_verified_boot(IsVerifiedMode());
  }
}

void AttestationService::Verify(
    const VerifyRequest& request,
    const VerifyCallback& callback) {
  auto result = std::make_shared<VerifyReply>();
  base::Closure task =
      base::Bind(&AttestationService::VerifyTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<VerifyReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::VerifyTask(
    const VerifyRequest& request,
    const std::shared_ptr<VerifyReply>& result) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  result->set_status(STATUS_NOT_SUPPORTED);
}

void AttestationService::CreateEnrollRequest(
    const CreateEnrollRequestRequest& request,
    const CreateEnrollRequestCallback& callback) {
  auto result = std::make_shared<CreateEnrollRequestReply>();
  base::Closure task =
      base::Bind(&AttestationService::CreateEnrollRequestTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<CreateEnrollRequestReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::CreateEnrollRequestTask(
    const CreateEnrollRequestRequest& request,
    const std::shared_ptr<CreateEnrollRequestReply>& result) {
  if (!CreateEnrollRequestInternal(result->mutable_pca_request())) {
    result->clear_pca_request();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
  }
}

void AttestationService::FinishEnroll(
    const FinishEnrollRequest& request,
    const FinishEnrollCallback& callback) {
  auto result = std::make_shared<FinishEnrollReply>();
  base::Closure task =
      base::Bind(&AttestationService::FinishEnrollTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<FinishEnrollReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::FinishEnrollTask(
    const FinishEnrollRequest& request,
    const std::shared_ptr<FinishEnrollReply>& result) {
  std::string server_error;
  if (!FinishEnrollInternal(request.pca_response(), &server_error)) {
    if (server_error.empty()) {
      LOG(ERROR) << __func__ << ": Server error";
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    } else {
      LOG(ERROR) << __func__ << ": Server error details: " << server_error;
      result->set_status(STATUS_REQUEST_DENIED_BY_CA);
    }
  }
}

void AttestationService::CreateCertificateRequest(
    const CreateCertificateRequestRequest& request,
    const CreateCertificateRequestCallback& callback) {
  auto result = std::make_shared<CreateCertificateRequestReply>();
  base::Closure task =
      base::Bind(&AttestationService::CreateCertificateRequestTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<CreateCertificateRequestReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::CreateCertificateRequestTask(
    const CreateCertificateRequestRequest& request,
    const std::shared_ptr<CreateCertificateRequestReply>& result) {
  std::string key_label;
  if (!crypto_utility_->GetRandom(kNonceSize, &key_label)) {
    LOG(ERROR) << __func__ << ": GetRandom(message_id) failed.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  std::string nonce;
  if (!crypto_utility_->GetRandom(kNonceSize, &nonce)) {
    LOG(ERROR) << __func__ << ": GetRandom(nonce) failed.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  std::string key_blob;
  std::string public_key;
  std::string public_key_tpm_format;
  std::string key_info;
  std::string proof;
  auto database_pb = database_->GetProtobuf();
  CertifiedKey key;
  if (!tpm_utility_->CreateCertifiedKey(
          KEY_TYPE_RSA, KEY_USAGE_SIGN,
          database_pb.identity_key().identity_key_blob(), nonce, &key_blob,
          &public_key, &public_key_tpm_format, &key_info, &proof)) {
    LOG(ERROR) << __func__ << ": Failed to create a key.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  key.set_key_blob(key_blob);
  key.set_public_key(public_key);
  key.set_key_name(key_label);
  key.set_public_key_tpm_format(public_key_tpm_format);
  key.set_certified_key_info(key_info);
  key.set_certified_key_proof(proof);
  key.set_key_type(KEY_TYPE_RSA);
  key.set_key_usage(KEY_USAGE_SIGN);
  std::string message_id;
  if (!CreateCertificateRequestInternal(request.username(), key,
                                        request.certificate_profile(),
                                        request.request_origin(),
                                        result->mutable_pca_request(),
                                        &message_id)) {
    result->clear_pca_request();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  std::string serialized_key;
  if (!key.SerializeToString(&serialized_key)) {
    LOG(ERROR) << __func__ << ": Failed to serialize key protobuf.";
    result->clear_pca_request();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  pending_cert_requests_[message_id] = serialized_key;
}

void AttestationService::FinishCertificateRequest(
    const FinishCertificateRequestRequest& request,
    const FinishCertificateRequestCallback& callback) {
  auto result = std::make_shared<FinishCertificateRequestReply>();
  base::Closure task =
      base::Bind(&AttestationService::FinishCertificateRequestTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<FinishCertificateRequestReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::FinishCertificateRequestTask(
    const FinishCertificateRequestRequest& request,
    const std::shared_ptr<FinishCertificateRequestReply>& result) {
  AttestationCertificateResponse response_pb;
  if (!response_pb.ParseFromString(request.pca_response())) {
    LOG(ERROR) << __func__ << ": Failed to parse response from Attestation CA.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  CertRequestMap::iterator iter = pending_cert_requests_.find(
      response_pb.message_id());
  if (iter == pending_cert_requests_.end()) {
    LOG(ERROR) << __func__ << ": Pending request not found.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  if (response_pb.status() != OK) {
    LOG(ERROR) << __func__ << ": Error received from Attestation CA: "
               << response_pb.detail();
    pending_cert_requests_.erase(iter);
    result->set_status(STATUS_REQUEST_DENIED_BY_CA);
    return;
  }
  CertifiedKey key;
  if (!key.ParseFromArray(iter->second.data(),
                          iter->second.size())) {
    LOG(ERROR) << __func__ << ": Failed to parse pending request key.";
    pending_cert_requests_.erase(iter);
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  pending_cert_requests_.erase(iter);
  if (!PopulateAndStoreCertifiedKey(response_pb, request.username(),
                                    request.key_label(), &key,
                                    result->mutable_certificate())) {
    result->clear_certificate();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
}

bool AttestationService::ValidateEnterpriseChallenge(
    const SignedData& signed_challenge) {
  const char kExpectedChallengePrefix[] = "EnterpriseKeyChallenge";
  if (!crypto_utility_->VerifySignatureUsingHexKey(
      kEnterpriseSigningPublicKey,
      signed_challenge.data(),
      signed_challenge.signature())) {
    LOG(ERROR) << __func__ << ": Failed to verify challenge signature.";
    return false;
  }
  Challenge challenge;
  if (!challenge.ParseFromString(signed_challenge.data())) {
    LOG(ERROR) << __func__ << ": Failed to parse challenge protobuf.";
    return false;
  }
  if (challenge.prefix() != kExpectedChallengePrefix) {
    LOG(ERROR) << __func__ << ": Unexpected challenge prefix.";
    return false;
  }
  return true;
}

bool AttestationService::EncryptEnterpriseKeyInfo(
    const KeyInfo& key_info,
    EncryptedData* encrypted_data) {
  std::string serialized;
  if (!key_info.SerializeToString(&serialized)) {
    LOG(ERROR) << "Failed to serialize key info.";
    return false;
  }
  std::string enterprise_key_id(
      kEnterpriseEncryptionPublicKeyID,
      arraysize(kEnterpriseEncryptionPublicKeyID) - 1);
  return crypto_utility_->EncryptDataForGoogle(
             serialized, kEnterpriseEncryptionPublicKey,
             enterprise_key_id, encrypted_data);
}

void AttestationService::SignEnterpriseChallenge(
    const SignEnterpriseChallengeRequest& request,
    const SignEnterpriseChallengeCallback& callback) {
  auto result = std::make_shared<SignEnterpriseChallengeReply>();
  base::Closure task =
      base::Bind(&AttestationService::SignEnterpriseChallengeTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<SignEnterpriseChallengeReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::SignEnterpriseChallengeTask(
    const SignEnterpriseChallengeRequest& request,
    const std::shared_ptr<SignEnterpriseChallengeReply>& result) {
  CertifiedKey key;
  if (!FindKeyByLabel(request.username(), request.key_label(), &key)) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }

  // Validate that the challenge is coming from the expected source.
  SignedData signed_challenge;
  if (!signed_challenge.ParseFromString(request.challenge())) {
    LOG(ERROR) << __func__ << ": Failed to parse signed challenge.";
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  if (!ValidateEnterpriseChallenge(signed_challenge)) {
    LOG(ERROR) << __func__ << ": Invalid challenge.";
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  // Add a nonce to ensure this service cannot be used to sign arbitrary data.
  std::string nonce;
  if (!crypto_utility_->GetRandom(kChallengeSignatureNonceSize, &nonce)) {
    LOG(ERROR) << __func__ << ": Failed to generate nonce.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }

  bool is_user_specific = request.has_username();
  KeyInfo key_info;
  // EUK -> Enterprise User Key
  // EMK -> Enterprise Machine Key
  key_info.set_key_type(is_user_specific ? EUK : EMK);
  key_info.set_domain(request.domain());
  key_info.set_device_id(request.device_id());
  // Only include the certificate if this is a user key.
  if (is_user_specific) {
    key_info.set_certificate(CreatePEMCertificateChain(key));
  }
  if (is_user_specific && request.include_signed_public_key()) {
    std::string spkac;
    if (!crypto_utility_->CreateSPKAC(key.key_blob(), key.public_key(),
                                      &spkac)) {
      LOG(ERROR) << __func__ << ": Failed to create signed public key.";
      result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
      return;
    }
    key_info.set_signed_public_key_and_challenge(spkac);
  }
  ChallengeResponse response_pb;
  *response_pb.mutable_challenge() = signed_challenge;
  response_pb.set_nonce(nonce);
  if (!EncryptEnterpriseKeyInfo(key_info,
                                response_pb.mutable_encrypted_key_info())) {
    LOG(ERROR) << __func__ << ": Failed to encrypt KeyInfo.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }

  // Serialize and sign the response protobuf.
  std::string serialized;
  if (!response_pb.SerializeToString(&serialized)) {
    LOG(ERROR) << __func__ << ": Failed to serialize response protobuf.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  if (!SignChallengeData(key, serialized,
                         result->mutable_challenge_response())) {
    result->clear_challenge_response();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
}

void AttestationService::SignSimpleChallenge(
    const SignSimpleChallengeRequest& request,
    const SignSimpleChallengeCallback& callback) {
  auto result = std::make_shared<SignSimpleChallengeReply>();
  base::Closure task =
      base::Bind(&AttestationService::SignSimpleChallengeTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<SignSimpleChallengeReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::SignSimpleChallengeTask(
    const SignSimpleChallengeRequest& request,
    const std::shared_ptr<SignSimpleChallengeReply>& result) {
  CertifiedKey key;
  if (!FindKeyByLabel(request.username(), request.key_label(), &key)) {
    result->set_status(STATUS_INVALID_PARAMETER);
    return;
  }
  // Add a nonce to ensure this service cannot be used to sign arbitrary data.
  std::string nonce;
  if (!crypto_utility_->GetRandom(kChallengeSignatureNonceSize, &nonce)) {
    LOG(ERROR) << __func__ << ": Failed to generate nonce.";
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
  if (!SignChallengeData(key, request.challenge() + nonce,
                         result->mutable_challenge_response())) {
    result->clear_challenge_response();
    result->set_status(STATUS_UNEXPECTED_DEVICE_ERROR);
    return;
  }
}

bool AttestationService::SignChallengeData(const CertifiedKey& key,
                                           const std::string& data_to_sign,
                                           std::string* response) {
  std::string signature;
  if (!tpm_utility_->Sign(key.key_blob(), data_to_sign, &signature)) {
    LOG(ERROR) << __func__ << ": Failed to sign data.";
    return false;
  }
  SignedData signed_data;
  signed_data.set_data(data_to_sign);
  signed_data.set_signature(signature);
  if (!signed_data.SerializeToString(response)) {
    LOG(ERROR) << __func__ << ": Failed to serialize signed data.";
    return false;
  }
  return true;
}

void AttestationService::SetKeyPayload(
    const SetKeyPayloadRequest& request,
    const SetKeyPayloadCallback& callback) {
  auto result = std::make_shared<SetKeyPayloadReply>();
  base::Closure task =
      base::Bind(&AttestationService::SetKeyPayloadTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<SetKeyPayloadReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::SetKeyPayloadTask(
    const SetKeyPayloadRequest& request,
    const std::shared_ptr<SetKeyPayloadReply>& result) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  result->set_status(STATUS_NOT_SUPPORTED);
}

void AttestationService::DeleteKeys(
    const DeleteKeysRequest& request,
    const DeleteKeysCallback& callback) {
  auto result = std::make_shared<DeleteKeysReply>();
  base::Closure task =
      base::Bind(&AttestationService::DeleteKeysTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<DeleteKeysReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::DeleteKeysTask(
    const DeleteKeysRequest& request,
    const std::shared_ptr<DeleteKeysReply>& result) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  result->set_status(STATUS_NOT_SUPPORTED);
}

void AttestationService::ResetIdentity(
    const ResetIdentityRequest& request,
    const ResetIdentityCallback& callback) {
  auto result = std::make_shared<ResetIdentityReply>();
  base::Closure task =
      base::Bind(&AttestationService::ResetIdentityTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<ResetIdentityReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::ResetIdentityTask(
    const ResetIdentityRequest& request,
    const std::shared_ptr<ResetIdentityReply>& result) {
  LOG(ERROR) << __func__ << ": Not implemented.";
  result->set_status(STATUS_NOT_SUPPORTED);
}

void AttestationService::SetSystemSalt(
    const SetSystemSaltRequest& request,
    const SetSystemSaltCallback& callback) {
  auto result = std::make_shared<SetSystemSaltReply>();
  base::Closure task =
      base::Bind(&AttestationService::SetSystemSaltTask,
                 base::Unretained(this), request, result);
  base::Closure reply = base::Bind(
      &AttestationService::TaskRelayCallback<SetSystemSaltReply>,
      GetWeakPtr(), callback, result);
  worker_thread_->task_runner()->PostTaskAndReply(FROM_HERE, task, reply);
}

void AttestationService::SetSystemSaltTask(
    const SetSystemSaltRequest& request,
    const std::shared_ptr<SetSystemSaltReply>& result) {
  system_salt_.assign(request.system_salt());
  brillo::cryptohome::home::SetSystemSalt(&system_salt_);
}

base::WeakPtr<AttestationService> AttestationService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace attestation

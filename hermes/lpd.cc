// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/lpd.h"

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>
#include <base/strings/string_number_conversions.h>

#include "hermes/qmi_constants.h"

namespace hermes {

// TODO(jruthe): remove header length once ASN.1 parsing is complete
constexpr uint8_t kChallengeHeaderLength = 5;
constexpr uint8_t kEsimChallengeLength = 16;

Lpd::Lpd(std::unique_ptr<Esim> esim, std::unique_ptr<Smdp> smdp)
    : esim_(std::move(esim)), smdp_(std::move(smdp)) {}

Lpd::~Lpd() = default;

void Lpd::InstallProfile(const SuccessCallback& success_callback,
                         const Lpd::LpdErrorCallback& error_callback) {
  user_success_ = success_callback;
  user_error_ = error_callback;
  esim_error_handler_ =
      base::Bind(&Lpd::HandleEsimError, base::Unretained(this), error_callback);
  smdp_error_handler_ =
      base::Bind(&Lpd::HandleSmdpError, base::Unretained(this), error_callback);
  Authenticate();
}

void Lpd::Initialize(const SuccessCallback& success_callback,
                     const Lpd::LpdErrorCallback& error_callback) {
  esim_->Initialize(success_callback, esim_error_handler_);
}

void Lpd::Authenticate() {
  esim_->OpenLogicalChannel(
      base::Bind(&Lpd::OnOpenLogicalChannel, base::Unretained(this)),
      esim_error_handler_);
}

void Lpd::OnOpenLogicalChannel(const std::vector<uint8_t>&) {
  esim_->GetInfo(kEsimInfo1Tag,
                 base::Bind(&Lpd::OnEsimInfoResult, base::Unretained(this)),
                 esim_error_handler_);
}

void Lpd::OnAuthenticateSuccess(const std::string& transaction_id,
                                const std::vector<uint8_t>& profile_metadata,
                                const std::vector<uint8_t>& smdp_signed2,
                                const std::vector<uint8_t>& smdp_signature2,
                                const std::vector<uint8_t>& smdp_certificate) {
  if (transaction_id != transaction_id_) {
    LOG(ERROR) << __func__ << ": transaction_id does not match";
    user_error_.Run(LpdError::kFailure);
    return;
  }
  // TODO(jruthe): DownloadProfile call
  esim_->PrepareDownloadRequest(
      smdp_signed2, smdp_signature2, smdp_certificate,
      base::Bind(&Lpd::OnPrepareDownloadRequest, base::Unretained(this)),
      esim_error_handler_);
}

void Lpd::OnPrepareDownloadRequest(const std::vector<uint8_t>& data) {
  smdp_->GetBoundProfilePackage(
      transaction_id_, data,
      base::Bind(&Lpd::OnGetBoundProfilePackage, base::Unretained(this)),
      smdp_error_handler_);
}

void Lpd::OnGetBoundProfilePackage(
    const std::string& transaction_id,
    const std::vector<uint8_t>& bound_profile_package) {
  if (transaction_id != transaction_id_) {
    LOG(ERROR) << __func__ << ": transaction id does not match";
    user_error_.Run(LpdError::kFailure);
    return;
  }

  VLOG(1) << __func__ << ": bound_profile_package : "
          << base::HexEncode(bound_profile_package.data(),
                             bound_profile_package.size());

  // TODO(jruthe): Install |bound_profile_package| through Esim interface
  user_success_.Run();
}

void Lpd::OnEsimInfoResult(const std::vector<uint8_t>& info) {
  esim_->GetChallenge(
      base::Bind(&Lpd::OnEsimChallengeResult, base::Unretained(this), info),
      esim_error_handler_);
}
void Lpd::OnLoadBoundProfilePackage(
    const std::vector<uint8_t>& profile_installation_result) {
  LOG(INFO) << __func__ << ": Profile installation succeeded";
  user_success_.Run();
}

void Lpd::OnEsimChallengeResult(const std::vector<uint8_t>& info1,
                                const std::vector<uint8_t>& challenge) {
  if (challenge.size() - kChallengeHeaderLength != kEsimChallengeLength) {
    user_error_.Run(LpdError::kFailure);
    return;
  }
  // TODO(jruthe): this is currently a trick to send only the value bytes of
  // the challenge to SmdpImpl, but should probably be parsed more correctly
  // here (along with most of the rest of the ASN.1 encoded data).
  smdp_->InitiateAuthentication(
      info1,
      std::vector<uint8_t>(challenge.begin() + 5 /* length of header */,
                           challenge.end()),
      base::Bind(&Lpd::OnInitiateAuthenticationResult, base::Unretained(this)),
      smdp_error_handler_);
}

void Lpd::OnInitiateAuthenticationResult(
    const std::string& transaction_id,
    const std::vector<uint8_t>& server_signed1,
    const std::vector<uint8_t>& server_signature1,
    const std::vector<uint8_t>& euicc_ci_pk_id_to_be_used,
    const std::vector<uint8_t>& server_certificate) {
  transaction_id_ = transaction_id;
  esim_->AuthenticateServer(
      server_signed1, server_signature1, euicc_ci_pk_id_to_be_used,
      server_certificate,
      base::Bind(&Lpd::OnAuthenticateServerResult, base::Unretained(this)),
      esim_error_handler_);
}

void Lpd::OnAuthenticateServerResult(
    const std::vector<uint8_t>& data) {
  smdp_->AuthenticateClient(
      transaction_id_, data,
      base::Bind(&Lpd::OnAuthenticateSuccess, base::Unretained(this)),
      smdp_error_handler_);
}

void Lpd::HandleEsimError(const Lpd::LpdErrorCallback& lpd_callback,
                          EsimError error) {
  switch (error) {
    case EsimError::kEsimSuccess:
      lpd_callback.Run(LpdError::kSuccess);
      break;
    case EsimError::kEsimError:
      lpd_callback.Run(LpdError::kFailure);
      break;
    case EsimError::kEsimNotConnected:
      lpd_callback.Run(LpdError::kRetry);
      break;
  }
}

// TODO(jruthe): Implement Smdp generic error handling.
void Lpd::HandleSmdpError(const Lpd::LpdErrorCallback& lpd_callback,
                          const std::vector<uint8_t>& smdp_error_data) {}

}  // namespace hermes

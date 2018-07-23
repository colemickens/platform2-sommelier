// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/lpd.h"

#include <utility>

#include <base/bind.h>
#include <base/callback.h>

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
      base::Bind(
          &Lpd::OnOpenLogicalChannel, base::Unretained(this),
          base::Bind(&Lpd::OnAuthenticateSuccess, base::Unretained(this))),
      esim_error_handler_);
}

void Lpd::OnOpenLogicalChannel(const Lpd::SuccessCallback& success_callback,
                               const std::vector<uint8_t>&) {
  esim_->GetInfo(kEsimInfo1Tag,
                 base::Bind(&Lpd::OnEsimInfoResult, base::Unretained(this),
                            success_callback),
                 esim_error_handler_);
}

void Lpd::OnAuthenticateSuccess() {
  // TODO(jruthe): DownloadProfile call
}

void Lpd::OnAuthenticateError(LpdError error) {
  user_error_.Run(error);
}

void Lpd::OnEsimInfoResult(const Lpd::SuccessCallback& success_callback,
                           const std::vector<uint8_t>& info) {
  esim_->GetChallenge(
      base::Bind(&Lpd::OnEsimChallengeResult, base::Unretained(this),
                 success_callback, info),
      esim_error_handler_);
}

void Lpd::OnEsimChallengeResult(const Lpd::SuccessCallback& success_callback,
                                const std::vector<uint8_t>& info1,
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
      base::Bind(&Lpd::OnInitiateAuthenticationResult, base::Unretained(this),
                 success_callback),
      smdp_error_handler_);
}

void Lpd::OnInitiateAuthenticationResult(
    const Lpd::SuccessCallback& success_callback,
    const std::vector<uint8_t>& server_signed1,
    const std::vector<uint8_t>& server_signature1,
    const std::vector<uint8_t>& public_keys_to_use,
    const std::vector<uint8_t>& server_certificate) {
  // TODO(jruthe): update parameters to Esim::AuthenticateServer
  esim_->AuthenticateServer(
      std::vector<uint8_t>(),
      base::Bind(&Lpd::OnAuthenticateServerResult, base::Unretained(this),
                 success_callback),
      esim_error_handler_);
}

void Lpd::OnAuthenticateServerResult(
    const Lpd::SuccessCallback& success_callback,
    const std::vector<uint8_t>& data) {
  success_callback.Run();
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

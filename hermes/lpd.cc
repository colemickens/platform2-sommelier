// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/lpd.h"

#include <utility>

#include <base/bind.h>

#include "hermes/qmi_constants.h"

namespace hermes {

Lpd::Lpd(std::unique_ptr<Esim> esim, std::unique_ptr<Smdp> smdp)
    : esim_(std::move(esim)), smdp_(std::move(smdp)) {}

Lpd::~Lpd() = default;

void Lpd::InstallProfile(const Lpd::SuccessCallback& success_callback,
                         const Lpd::ErrorCallback& error_callback) {
  Authenticate(success_callback, error_callback);
}

void Lpd::Authenticate(const Lpd::SuccessCallback& success_callback,
                       const Lpd::ErrorCallback& error_callback) {
  esim_->GetInfo(kEsimInfo1,
                 base::Bind(&Lpd::OnEsimInfoResult, base::Unretained(this),
                            success_callback, error_callback),
                 error_callback);
}

void Lpd::OnEsimInfoResult(const Lpd::SuccessCallback& success_callback,
                           const Lpd::ErrorCallback& error_callback,
                           const std::vector<uint8_t>& info) {
  esim_->GetChallenge(
      base::Bind(&Lpd::OnEsimChallengeResult, base::Unretained(this),
                 success_callback, error_callback, info),
      error_callback);
}

void Lpd::OnEsimChallengeResult(const Lpd::SuccessCallback& success_callback,
                                const Lpd::ErrorCallback& error_callback,
                                const std::vector<uint8_t>& info1,
                                const std::vector<uint8_t>& challenge) {
  smdp_->InitiateAuthentication(
      info1, challenge,
      base::Bind(&Lpd::OnInitiateAuthResult, base::Unretained(this),
                 success_callback, error_callback),
      error_callback);
}

void Lpd::OnInitiateAuthResult(const Lpd::SuccessCallback& success_callback,
                               const Lpd::ErrorCallback& error_callback,
                               const std::vector<uint8_t>& data) {
  esim_->AuthenticateServer(
      data,
      base::Bind(&Lpd::OnAuthServerResult, base::Unretained(this),
                 success_callback, error_callback),
      error_callback);
}

void Lpd::OnAuthServerResult(const Lpd::SuccessCallback& success_callback,
                             const Lpd::ErrorCallback& error_callback,
                             const std::vector<uint8_t>& data) {
  success_callback.Run(data);
}

}  // namespace hermes

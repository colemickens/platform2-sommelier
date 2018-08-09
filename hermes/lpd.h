// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_LPD_H_
#define HERMES_LPD_H_

#include <memory>
#include <string>
#include <vector>

#include <base/logging.h>
#include <base/macros.h>

#include "hermes/esim.h"
#include "hermes/smdp.h"

namespace hermes {

// TODO(jruthe): // For now this is a simple binary pass/fail model. More
// functionality will be built incrementally as QMI API is tested.
enum class LpdError {
  kSuccess,
  kFailure,
  kRetry,
};

// Provides a channel through which the eSIM chip communicates to a SM-DP+
// server to download and install a carrier profile in accordance to SGP.22
class Lpd {
 public:
  using LpdErrorCallback = base::Callback<void(LpdError error)>;
  using EsimErrorCallback = base::Callback<void(EsimError error)>;
  // TODO(jruthe): add an SmdpErrorGeneric enum for the Smdp interface
  using SmdpErrorCallback =
      base::Callback<void(const std::vector<uint8_t>& smdp_error)>;
  using SuccessCallback = base::Closure;

  Lpd(std::unique_ptr<Esim> esim, std::unique_ptr<Smdp> smpd);
  ~Lpd();

  void Initialize(const SuccessCallback& success_callback,
                  const LpdErrorCallback& error_callback);

  // Performs the Common Mutual Authentication Procedure as specified in SGP.22
  // section 3.1.2, as well as the Profile Download and Installation as
  // specified in SGP.22 section 3.1.3. There are three major steps to install a
  // carrier's profile:
  //    1. Authenticate the eSIM and the carrier's SM-DP+ server
  //    2. Download the profile from the server
  //    3. Install the profile to the eSIM chip
  // InstallProfile performs all of these steps and on successful return, will
  // guarantee that the requested profile has been loaded to the eSIM and can
  // be activated if desired.
  void InstallProfile(const SuccessCallback& success_callback,
                      const LpdErrorCallback& error_callback);

 private:
  // Performs the first step outlined for InstallProfile. As specified
  // in SGP.22 section 3.1.2, there are a few distinct steps to get
  // this done:
  //    1. Get the eSIM Info and Challenge
  //    2. Begin the authentication with the SM-DP+ server
  //    3. Perform the key exchange between the server and chip
  // Once all of these steps have completed, there is a secure channel between
  // the eSIM and SM-DP+ server, and the Profile Download and Installation
  // procedure can be executed.
  void Authenticate();

  void OnAuthenticateSuccess(const std::string& transaction_id,
                             const std::vector<uint8_t>& profile_metadata,
                             const std::vector<uint8_t>& smdp_signed2,
                             const std::vector<uint8_t>& smdp_signature2,
                             const std::vector<uint8_t>& smdp_certificate);

  void OnAuthenticateError(LpdError error);

  void OnOpenLogicalChannel(const std::vector<uint8_t>&);
  void OnEsimInfoResult(const std::vector<uint8_t>& info);
  void OnEsimChallengeResult(const std::vector<uint8_t>& info1,
                             const std::vector<uint8_t>& challenge);
  void OnInitiateAuthenticationResult(
      const std::string& transaction_id,
      const std::vector<uint8_t>& server_signed1,
      const std::vector<uint8_t>& server_signature1,
      const std::vector<uint8_t>& euicc_ci_pk_id_to_be_used,
      const std::vector<uint8_t>& server_certificate);
  void OnAuthenticateServerResult(const std::vector<uint8_t>& data);
  void OnPrepareDownloadRequest(const std::vector<uint8_t>& data);
  void OnGetBoundProfilePackage(
      const std::string& transaction_id,
      const std::vector<uint8_t>& bound_profile_package);
  void OnLoadBoundProfilePackage(
      const std::vector<uint8_t>& profile_installation_result);

  void HandleEsimError(const LpdErrorCallback& lpd_callback,
                       EsimError esim_error);

  void HandleSmdpError(const LpdErrorCallback& lpd_callback,
                       const std::vector<uint8_t>& smdp_error_data);

  std::string transaction_id_;

  std::unique_ptr<Esim> esim_;
  std::unique_ptr<Smdp> smdp_;

  SuccessCallback user_success_;
  LpdErrorCallback user_error_;
  EsimErrorCallback esim_error_handler_;
  SmdpErrorCallback smdp_error_handler_;

  DISALLOW_COPY_AND_ASSIGN(Lpd);
};

}  // namespace hermes

#endif  // HERMES_LPD_H_

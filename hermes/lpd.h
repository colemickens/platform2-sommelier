// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_LPD_H_
#define HERMES_LPD_H_

#include <memory>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>

#include "hermes/esim.h"
#include "hermes/smdp.h"

namespace hermes {

// Provides a channel through which the eSIM chip communicates to a SM-DP+
// server to download and install a carrier profile in accordance to SGP.22
class Lpd {
 public:
  using ErrorCallback =
      base::Callback<void(const std::vector<uint8_t>& error_data)>;
  using SuccessCallback =
      base::Callback<void(const std::vector<uint8_t>& success_data)>;

  Lpd(std::unique_ptr<Esim> esim, std::unique_ptr<Smdp> smpd);
  ~Lpd();

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
                      const ErrorCallback& error_callback);

 private:
  // Performs the first step outlined for InstallProfile. As specified in SGP.22
  // section 3.1.2, there are a few distinct steps to get this done:
  //    1. Get the eSIM Info and Challenge
  //    2. Begin the authentication with the SM-DP+ server
  //    3. Perform the key exchange between the server and chip
  // Once all of these steps have completed, there is a secure channel between
  // the eSIM and SM-DP+ server, and the Profile Download and Installation
  // procedure can be executed.
  void Authenticate(const SuccessCallback& success_callback,
                    const ErrorCallback& error_callback);

  void OnEsimInfoResult(const SuccessCallback& success_callback,
                        const ErrorCallback& error_callback,
                        const std::vector<uint8_t>& info);
  void OnEsimChallengeResult(const SuccessCallback& success_callback,
                             const ErrorCallback& error_callback,
                             const std::vector<uint8_t>& info1,
                             const std::vector<uint8_t>& challenge);
  void OnAuthServerResult(const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback,
                          const std::vector<uint8_t>& data);
  void OnInitiateAuthResult(const SuccessCallback& success_callback,
                            const ErrorCallback& error_callback,
                            const std::vector<uint8_t>& data);
  void OnAuthClientResult(const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback,
                          const std::vector<uint8_t>& data);

  std::unique_ptr<Esim> esim_;
  std::unique_ptr<Smdp> smdp_;

  DISALLOW_COPY_AND_ASSIGN(Lpd);
};

}  // namespace hermes

#endif  // HERMES_LPD_H_

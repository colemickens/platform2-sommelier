// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_SMDP_H_
#define HERMES_SMDP_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/values.h>
#include <brillo/http/http_request.h>
#include <brillo/http/http_transport.h>

#include "hermes/smdp_interface.h"

namespace hermes {

// Class to facilitate communication between the LPD and SM-DP+
// server. Responsible for opening, maintaining, and closing an
// HTTPS connection with the SM-DP+ server.
class Smdp : public SmdpInterface{
 public:
  using InitiateAuthenticationCallback =
    base::Callback<void(const std::string& transaction_id,
                        const std::vector<uint8_t>& server_signed1,
                        const std::vector<uint8_t>& server_signature1,
                        const std::vector<uint8_t>& euicc_ci_pk_id_to_be_used,
                        const std::vector<uint8_t>& server_certificate)>;
  using AuthenticateClientCallback =
    base::Callback<void(const std::string& transaction_id,
                        const std::vector<uint8_t>& profile_metadata,
                        const std::vector<uint8_t>& smdp_signed2,
                        const std::vector<uint8_t>& smdp_signature2,
                        const std::vector<uint8_t>& public_key)>;
  using GetBoundProfilePackageCallback =
    base::Callback<void(const std::string& transaction_id,
                        const std::vector<uint8_t>& bound_profile_package)>;
  using ErrorCallback =
      base::Callback<void(const std::vector<uint8_t>& error_data)>;
  using DictionaryPtr = std::unique_ptr<base::DictionaryValue>;

  explicit Smdp(const std::string& server_hostname);
  ~Smdp() = default;

  // First, establishes a connection to the SM-DP+ server over which
  // the ES8+ secure channel will be tunneled, then sends server the eSIM
  // challenge and info1 to begin the Authentication procedure. |callback| is
  // called upon server's response, or |error_callback| is called on server
  // error.
  //
  // Parameters
  //    challenge - eSIM challenge as returned by Esim.GetEuiccChallenge
  //    info1 - eSIM info1 as returned by Esim.GetEuiccInfo
  void InitiateAuthentication(
      const std::vector<uint8_t>& info1,
      const std::vector<uint8_t>& challenge,
      const InitiateAuthenticationCallback& data_callback,
      const ErrorCallback& error_callback);

  void AuthenticateClient(const std::string& transaction_id,
                          const std::vector<uint8_t>& data,
                          const AuthenticateClientCallback& data_callback,
                          const ErrorCallback& error_callback);

  void GetBoundProfilePackage(
      const std::string& transaction_id,
      const std::vector<uint8_t>& data,
      const GetBoundProfilePackageCallback& data_callback,
      const ErrorCallback& error_callback);

 private:
  void OnHttpResponse(const base::Callback<void(DictionaryPtr)>& data_callback,
                      const ErrorCallback& error_callback,
                      brillo::http::RequestID request_id,
                      std::unique_ptr<brillo::http::Response> response);
  void OnHttpError(const ErrorCallback& error_callback,
                   brillo::http::RequestID request_id,
                   const brillo::Error* error);
  void OnInitiateAuthenticationResponse(
      const InitiateAuthenticationCallback& data_callback,
      const ErrorCallback& error_callback,
      DictionaryPtr json_dict);
  void OnAuthenticateClientResponse(
      const AuthenticateClientCallback& data_callback,
      const ErrorCallback& error_callback,
      DictionaryPtr json_dict);
  void OnGetBoundProfilePackageResponse(
      const GetBoundProfilePackageCallback& data_callback,
      const ErrorCallback& error_callback,
      DictionaryPtr json_dict);

  void SendJsonRequest(const std::string& url,
                       const std::string& json_data,
                       const base::Callback<void(DictionaryPtr)>& data_callback,
                       const ErrorCallback& error_callback);

  const std::string server_hostname_;
  std::shared_ptr<brillo::http::Transport> server_transport_;
  base::WeakPtrFactory<Smdp> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Smdp);
};

}  // namespace hermes

#endif  // HERMES_SMDP_H_

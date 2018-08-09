// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HERMES_SMDP_IMPL_H_
#define HERMES_SMDP_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include <brillo/http/http_request.h>
#include <brillo/http/http_transport.h>

#include "hermes/smdp.h"

namespace hermes {

class SmdpImpl : public Smdp {
 public:
  explicit SmdpImpl(const std::string& server_hostname);
  ~SmdpImpl() = default;

  void InitiateAuthentication(
      const std::vector<uint8_t>& info1,
      const std::vector<uint8_t>& challenge,
      const InitiateAuthenticationCallback& data_callback,
      const ErrorCallback& error_callback) override;

  void AuthenticateClient(const std::string& transaction_id,
                          const std::vector<uint8_t>& data,
                          const AuthenticateClientCallback& data_callback,
                          const ErrorCallback& error_callback) override;

  void GetBoundProfilePackage(
      const std::string& transaction_id,
      const std::vector<uint8_t>& data,
      const GetBoundProfilePackageCallback& data_callback,
      const ErrorCallback& error_callback) override;

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
  base::WeakPtrFactory<SmdpImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SmdpImpl);
};

}  // namespace hermes

#endif  // HERMES_SMDP_IMPL_H_

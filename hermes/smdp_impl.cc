// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/smdp_impl.h"

#include <string>
#include <utility>

#include <base/base64.h>
#include <base/bind.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/values.h>

namespace hermes {

SmdpImpl::SmdpImpl(const std::string& server_hostname)
    : server_hostname_(server_hostname),
      server_transport_(brillo::http::Transport::CreateDefault()),
      weak_factory_(this) {}

void SmdpImpl::InitiateAuthentication(
    const std::vector<uint8_t>& info1,
    const std::vector<uint8_t>& challenge,
    const InitiateAuthenticationCallback& data_callback,
    const ErrorCallback& error_callback) {
  std::string encoded_info1(info1.begin(), info1.end());
  base::Base64Encode(encoded_info1, &encoded_info1);
  std::string encoded_challenge(challenge.begin(), challenge.end());
  base::Base64Encode(encoded_challenge, &encoded_challenge);

  base::DictionaryValue http_params;
  http_params.SetString("euiccInfo1", encoded_info1);
  http_params.SetString("euiccChallenge", encoded_challenge);
  http_params.SetString("smdpAddress", server_hostname_);
  std::string http_body;
  base::JSONWriter::Write(http_params, &http_body);

  SendJsonRequest(
      "https://" + server_hostname_ +
          "/gsma/rsp2/es9plus/initiateAuthentication",
      http_body,
      base::Bind(&SmdpImpl::OnInitiateAuthenticationResponse,
                 weak_factory_.GetWeakPtr(), data_callback, error_callback),
      error_callback);
}

void SmdpImpl::OnHttpResponse(
    const base::Callback<void(DictionaryPtr)>& data_callback,
    const ErrorCallback& error_callback,
    brillo::http::RequestID request_id,
    std::unique_ptr<brillo::http::Response> response) {
  if (!response) {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }

  std::string raw_data = response->ExtractDataAsString();

  LOG(INFO) << raw_data;

  std::unique_ptr<base::Value> result = base::JSONReader::Read(raw_data);
  if (result) {
    data_callback.Run(base::DictionaryValue::From(std::move(result)));
  } else {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }
}

void SmdpImpl::OnInitiateAuthenticationResponse(
    const InitiateAuthenticationCallback& data_callback,
    const ErrorCallback& error_callback,
    std::unique_ptr<base::DictionaryValue> json_dict) {
  std::string encoded_buffer;
  std::string server_signed1;
  if (!json_dict->GetString("serverSigned1", &encoded_buffer)) {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }
  base::Base64Decode(encoded_buffer, &server_signed1);

  std::string server_signature1;
  if (!json_dict->GetString("serverSignature1", &encoded_buffer)) {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }
  base::Base64Decode(encoded_buffer, &server_signature1);

  std::string euicc_ci_pk_id_to_be_used;
  if (!json_dict->GetString("euiccCiPKIdToBeUsed", &encoded_buffer)) {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }
  base::Base64Decode(encoded_buffer, &euicc_ci_pk_id_to_be_used);

  std::string server_certificate;
  if (!json_dict->GetString("serverCertificate", &encoded_buffer)) {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }
  base::Base64Decode(encoded_buffer, &server_certificate);

  std::string transaction_id;
  if (!json_dict->GetString("transactionId", &transaction_id)) {
    error_callback.Run(std::vector<uint8_t>());
    return;
  }

  data_callback.Run(
      transaction_id,
      std::vector<uint8_t>(server_signed1.begin(), server_signed1.end()),
      std::vector<uint8_t>(server_signature1.begin(), server_signature1.end()),
      std::vector<uint8_t>(euicc_ci_pk_id_to_be_used.begin(),
                           euicc_ci_pk_id_to_be_used.end()),
      std::vector<uint8_t>(server_certificate.begin(),
                           server_certificate.end()));
}

void SmdpImpl::OnHttpError(const ErrorCallback& error_callback,
                           brillo::http::RequestID request_id,
                           const brillo::Error* error) {
  LOG(INFO) << "here";
  error_callback.Run(std::vector<uint8_t>());
}

void SmdpImpl::SendJsonRequest(
    const std::string& url,
    const std::string& json_data,
    const base::Callback<void(DictionaryPtr)>& data_callback,
    const ErrorCallback& error_callback) {
  LOG(INFO) << "sending : " << json_data;
  brillo::ErrorPtr error = nullptr;
  brillo::http::Request request(url, brillo::http::request_type::kPost,
                                server_transport_);
  request.SetContentType("application/json");
  request.SetUserAgent("gsma-rsp-lpad");
  request.AddHeader("X-Admin-Protocol", "gsma/rsp/v2.0.0");
  request.AddRequestBody(json_data.data(), json_data.size(), &error);
  CHECK(!error);
  request.GetResponse(
      base::Bind(&SmdpImpl::OnHttpResponse, weak_factory_.GetWeakPtr(),
                 data_callback, error_callback),
      base::Bind(&SmdpImpl::OnHttpError, weak_factory_.GetWeakPtr(),
                 error_callback));
}

void SmdpImpl::AuthenticateClient(std::string transaction_id,
                                  const std::vector<uint8_t>& data,
                                  const base::Closure& data_callback,
                                  const ErrorCallback& error_callback) {
  std::string encoded_data(data.begin(), data.end());
  base::Base64Encode(encoded_data, &encoded_data);

  base::DictionaryValue http_params;
  http_params.SetString("transactionId", transaction_id);
  http_params.SetString("authenticateServerResponse", encoded_data);
  std::string http_body;
  base::JSONWriter::Write(http_params, &http_body);

  SendJsonRequest(
      "https://" + server_hostname_ + "/gsma/rsp2/es9plus/authenticateClient",
      http_body,
      base::Bind(&SmdpImpl::OnAuthenticateClientResponse,
                 weak_factory_.GetWeakPtr(), data_callback, error_callback),
      error_callback);
}

void SmdpImpl::OnAuthenticateClientResponse(
    const base::Closure& success_callback,
    const ErrorCallback& error_callback,
    DictionaryPtr json_dict) {
  LOG(INFO) << "Client authenticated successfully";
  std::string encoded_buffer;
  std::string smdp_signed2;
  if (!json_dict->GetString("smdpSigned2", &encoded_buffer)) {
    LOG(ERROR) << __func__ << ": smdpSigned2 not received";
    error_callback.Run(std::vector<uint8_t>());
    return;
  }
  base::Base64Decode(encoded_buffer, &smdp_signed2);

  success_callback.Run();
}

}  // namespace hermes

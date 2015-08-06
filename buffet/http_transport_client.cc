// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/http_transport_client.h"

#include <base/bind.h>
#include <chromeos/errors/error.h>
#include <chromeos/http/http_request.h>
#include <chromeos/http/http_utils.h>

namespace buffet {

namespace {

// The number of seconds each HTTP request will be allowed before timing out.
const int kRequestTimeoutSeconds = 30;

class ResponseImpl : public weave::HttpClient::Response {
 public:
  ~ResponseImpl() override = default;
  explicit ResponseImpl(std::unique_ptr<chromeos::http::Response> response)
      : response_{std::move(response)},
        data_{response_->ExtractDataAsString()} {}

  // weave::HttpClient::Response implementation
  int GetStatusCode() const override { return response_->GetStatusCode(); }

  std::string GetContentType() const override {
    return response_->GetContentType();
  }

  const std::string& GetData() const override { return data_; }

 private:
  std::unique_ptr<chromeos::http::Response> response_;
  std::string data_;
  DISALLOW_COPY_AND_ASSIGN(ResponseImpl);
};

void OnSuccessCallback(
    const weave::HttpClient::SuccessCallback& success_callback,
    int id,
    std::unique_ptr<chromeos::http::Response> response) {
  success_callback.Run(id, ResponseImpl{std::move(response)});
}

void OnErrorCallback(const weave::HttpClient::ErrorCallback& error_callback,
                     int id,
                     const chromeos::Error* error) {
  error_callback.Run(id, error);
}

}  // anonymous namespace

HttpTransportClient::HttpTransportClient()
    : transport_{chromeos::http::Transport::CreateDefault()} {
  transport_->SetDefaultTimeout(
      base::TimeDelta::FromSeconds(kRequestTimeoutSeconds));
}

HttpTransportClient::~HttpTransportClient() {}

std::unique_ptr<weave::HttpClient::Response>
HttpTransportClient::SendRequestAndBlock(const std::string& method,
                                         const std::string& url,
                                         const std::string& data,
                                         const std::string& mime_type,
                                         const Headers& headers,
                                         chromeos::ErrorPtr* error) {
  return std::unique_ptr<weave::HttpClient::Response>{
      new ResponseImpl{chromeos::http::SendRequestAndBlock(
          method, url, data.data(), data.size(), mime_type, headers, transport_,
          error)}};
}

int HttpTransportClient::SendRequest(const std::string& method,
                                     const std::string& url,
                                     const std::string& data,
                                     const std::string& mime_type,
                                     const Headers& headers,
                                     const SuccessCallback& success_callback,
                                     const ErrorCallback& error_callback) {
  return chromeos::http::SendRequest(
      method, url, data.data(), data.size(), mime_type, headers, transport_,
      base::Bind(&OnSuccessCallback, success_callback),
      base::Bind(&OnErrorCallback, error_callback));
}

}  // namespace buffet

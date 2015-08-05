// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <weave/mock_http_client.h>

#include <memory>
#include <string>

namespace weave {
namespace unittests {

std::unique_ptr<HttpClient::Response> MockHttpClient::SendRequestAndBlock(
    const std::string& method,
    const std::string& url,
    const std::string& data,
    const std::string& mime_type,
    const Headers& headers,
    chromeos::ErrorPtr* error) {
  return std::unique_ptr<Response>{
      MockSendRequest(method, url, data, mime_type, headers, error)};
}

int MockHttpClient::SendRequest(const std::string& method,
                                const std::string& url,
                                const std::string& data,
                                const std::string& mime_type,
                                const Headers& headers,
                                const SuccessCallback& success_callback,
                                const ErrorCallback& error_callback) {
  chromeos::ErrorPtr error;
  std::unique_ptr<Response> response{
      MockSendRequest(method, url, data, mime_type, headers, &error)};
  if (response) {
    success_callback.Run(0, *response);
  } else {
    error_callback.Run(0, error.get());
  }
  return 0;
}

}  // namespace unittests
}  // namespace weave

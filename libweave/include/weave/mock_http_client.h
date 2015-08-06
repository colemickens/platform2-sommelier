// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_MOCK_HTTP_CLIENT_H_
#define LIBWEAVE_INCLUDE_WEAVE_MOCK_HTTP_CLIENT_H_

#include <weave/http_client.h>

#include <memory>
#include <string>

#include <gmock/gmock.h>

namespace weave {
namespace unittests {

class MockHttpClientResponse : public HttpClient::Response {
 public:
  MOCK_CONST_METHOD0(GetStatusCode, int());
  MOCK_CONST_METHOD0(GetContentType, std::string());
  MOCK_CONST_METHOD0(GetData, const std::string&());
};

class MockHttpClient : public HttpClient {
 public:
  ~MockHttpClient() override = default;

  MOCK_METHOD5(MockSendRequest,
               Response*(const std::string&,
                         const std::string&,
                         const Headers&,
                         const std::string&,
                         chromeos::ErrorPtr*));

  std::unique_ptr<Response> SendRequestAndBlock(
      const std::string& method,
      const std::string& url,
      const Headers& headers,
      const std::string& data,
      chromeos::ErrorPtr* error) override;

  int SendRequest(const std::string& method,
                  const std::string& url,
                  const Headers& headers,
                  const std::string& data,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;
};

}  // namespace unittests
}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_MOCK_HTTP_CLIENT_H_

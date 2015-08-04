// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_HTTP_TRANSPORT_CLIENT_H_
#define LIBWEAVE_SRC_HTTP_TRANSPORT_CLIENT_H_

#include <memory>
#include <string>

#include <weave/http_client.h>

namespace chromeos {
namespace http {
class Transport;
}
}

namespace buffet {

class HttpTransportClient : public weave::HttpClient {
 public:
  explicit HttpTransportClient(
      const std::shared_ptr<chromeos::http::Transport>& transport);

  ~HttpTransportClient() override;

  // weave::HttpClient implementation.
  std::unique_ptr<Response> SendRequestAndBlock(
      const std::string& method,
      const std::string& url,
      const std::string& data,
      const std::string& mime_type,
      const Headers& headers,
      chromeos::ErrorPtr* error) override;

  int SendRequest(const std::string& method,
                  const std::string& url,
                  const std::string& data,
                  const std::string& mime_type,
                  const Headers& headers,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

 private:
  std::shared_ptr<chromeos::http::Transport> transport_;
  DISALLOW_COPY_AND_ASSIGN(HttpTransportClient);
};

}  // namespace buffet

#endif  // LIBWEAVE_SRC_HTTP_TRANSPORT_CLIENT_H_

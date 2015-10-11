// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_HTTP_TRANSPORT_CLIENT_H_
#define BUFFET_HTTP_TRANSPORT_CLIENT_H_

#include <memory>
#include <string>

#include <weave/provider/http_client.h>

namespace chromeos {
namespace http {
class Transport;
}
}

namespace buffet {

class HttpTransportClient : public weave::provider::HttpClient {
 public:
  HttpTransportClient();

  ~HttpTransportClient() override;

  void SendRequest(Method method,
                   const std::string& url,
                   const Headers& headers,
                   const std::string& data,
                   const SuccessCallback& success_callback,
                   const weave::ErrorCallback& error_callback) override;

 private:
  std::shared_ptr<chromeos::http::Transport> transport_;
  DISALLOW_COPY_AND_ASSIGN(HttpTransportClient);
};

}  // namespace buffet

#endif  // BUFFET_HTTP_TRANSPORT_CLIENT_H_

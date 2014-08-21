// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_HTTP_TRANSPORT_CURL_H_
#define LIBCHROMEOS_CHROMEOS_HTTP_TRANSPORT_CURL_H_

#include <string>

#include <chromeos/http_transport.h>

namespace chromeos {
namespace http {
namespace curl {

extern const char kErrorDomain[];

///////////////////////////////////////////////////////////////////////////////
// An implementation of http::Transport that uses libcurl for
// HTTP communications. This class (as http::Transport base)
// is used by http::Request and http::Response classes to provide HTTP
// functionality to the clients.
// See http_transport.h for more details.
///////////////////////////////////////////////////////////////////////////////
class Transport : public http::Transport {
 public:
  Transport();
  virtual ~Transport();

  std::unique_ptr<http::Connection> CreateConnection(
      std::shared_ptr<http::Transport> transport,
      const std::string& url,
      const std::string& method,
      const HeaderList& headers,
      const std::string& user_agent,
      const std::string& referer,
      chromeos::ErrorPtr* error) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Transport);
};

}  // namespace curl
}  // namespace http
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_HTTP_TRANSPORT_CURL_H_

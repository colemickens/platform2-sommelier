// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_transport_curl.h>

#include <base/logging.h>
#include <chromeos/http/http_connection_curl.h>
#include <chromeos/http/http_request.h>

namespace chromeos {
namespace http {
namespace curl {

const char kErrorDomain[] = "http_transport";

Transport::Transport() {
  VLOG(1) << "curl::Transport created";
}

Transport::~Transport() {
  VLOG(1) << "curl::Transport destroyed";
}

std::unique_ptr<http::Connection> Transport::CreateConnection(
    std::shared_ptr<http::Transport> transport,
    const std::string& url,
    const std::string& method,
    const HeaderList& headers,
    const std::string& user_agent,
    const std::string& referer,
    chromeos::ErrorPtr* error) {
  CURL* curl_handle = curl_easy_init();
  if (!curl_handle) {
    LOG(ERROR) << "Failed to initialize CURL";
    chromeos::Error::AddTo(error, http::curl::kErrorDomain, "curl_init_failed",
                           "Failed to initialize CURL");
    return std::unique_ptr<http::Connection>();
  }

  LOG(INFO) << "Sending a " << method << " request to " << url;
  curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());

  if (!user_agent.empty()) {
    curl_easy_setopt(curl_handle,
                     CURLOPT_USERAGENT, user_agent.c_str());
  }

  if (!referer.empty()) {
    curl_easy_setopt(curl_handle,
                     CURLOPT_REFERER, referer.c_str());
  }

  // Setup HTTP request method and optional request body.
  if (method ==  request_type::kGet) {
    curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);
  } else if (method == request_type::kHead) {
    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
  } else if (method == request_type::kPut) {
    curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1L);
  } else {
    // POST and custom request methods
    curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, nullptr);
    if (method != request_type::kPost)
      curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, method.c_str());
  }

  std::unique_ptr<http::Connection> connection(
      new http::curl::Connection(curl_handle, method, transport));
  CHECK(connection) << "Unable to create Connection object";
  if (!connection->SendHeaders(headers, error)) {
    connection.reset();
  }
  return connection;
}

}  // namespace curl
}  // namespace http
}  // namespace chromeos

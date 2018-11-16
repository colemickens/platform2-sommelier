// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_HTTP_REQUEST_H_
#define SHILL_HTTP_REQUEST_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/cancelable_callback.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <brillo/errors/error.h>
#include <brillo/http/http_transport.h>

#include "shill/net/byte_string.h"

#include "shill/net/shill_time.h"
#include "shill/refptr_types.h"

namespace shill {

class DnsClient;
class Error;
class EventDispatcher;
class HttpUrl;
class IPAddress;

// The HttpRequest class implements facilities for performing
// a simple "GET" request and returning the contents via a
// callback.
class HttpRequest {
 public:
  enum Result {
    kResultUnknown,
    kResultInvalidInput,
    kResultInProgress,
    kResultDNSFailure,
    kResultDNSTimeout,
    kResultConnectionFailure,
    kResultHTTPFailure,
    kResultHTTPTimeout,
    kResultSuccess
  };

  HttpRequest(ConnectionRefPtr connection, EventDispatcher* dispatcher);
  virtual ~HttpRequest();

  // Start an http GET request to the URL |url|. If the request succeeds,
  // |request_success_callback| is called with the response data.
  // Otherwise, request_error_callback is called with the error reason.
  //
  // This (Start) function returns kResultDNSFailure  if the request fails to
  // initialize the DNS client, or kResultInProgress if the request
  // has started successfully and is now in progress.
  virtual Result Start(
      const std::string& url_string,
      const base::Callback<void(std::shared_ptr<brillo::http::Response>)>&
          request_success_callback,
      const base::Callback<void(Result)>& request_error_callback);

  // Stop the current HttpRequest.  No callback is called as a side
  // effect of this function.
  virtual void Stop();

 private:
  friend class HttpRequestTest;

  // Time to wait for HTTP request.
  static const int kRequestTimeoutSeconds;

  void GetDNSResult(const Error& error, const IPAddress& address);
  void StartRequest();
  void SuccessCallback(brillo::http::RequestID request_id,
                       std::unique_ptr<brillo::http::Response> response);
  void ErrorCallback(brillo::http::RequestID request_id,
                     const brillo::Error* error);
  void SendStatus(Result result);

  ConnectionRefPtr connection_;

  base::WeakPtrFactory<HttpRequest> weak_ptr_factory_;
  base::Callback<void(const Error&, const IPAddress&)> dns_client_callback_;
  base::Callback<void(Result)> request_error_callback_;
  base::Callback<void(std::shared_ptr<brillo::http::Response>)>
      request_success_callback_;
  brillo::http::SuccessCallback success_callback_;
  brillo::http::ErrorCallback error_callback_;
  std::unique_ptr<DnsClient> dns_client_;
  std::shared_ptr<brillo::http::Transport> transport_;
  brillo::http::RequestID request_id_;
  std::string url_string_;
  std::string server_hostname_;
  int server_port_;
  std::string server_path_;
  bool is_running_;

  DISALLOW_COPY_AND_ASSIGN(HttpRequest);
};

}  // namespace shill

#endif  // SHILL_HTTP_REQUEST_H_

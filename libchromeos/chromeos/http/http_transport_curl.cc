// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_transport_curl.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop_proxy.h>
#include <chromeos/http/http_connection_curl.h>
#include <chromeos/http/http_request.h>
#include <chromeos/strings/string_utils.h>

namespace chromeos {
namespace http {
namespace curl {

Transport::Transport()
    : task_runner_{base::MessageLoopProxy::current()} {
  VLOG(1) << "curl::Transport created";
}

Transport::Transport(scoped_refptr<base::TaskRunner> task_runner)
    : task_runner_{task_runner} {
  VLOG(1) << "curl::Transport created";
}

Transport::Transport(const std::string& proxy)
    : proxy_{proxy},
      task_runner_{base::MessageLoopProxy::current()} {
  VLOG(1) << "curl::Transport created with proxy " << proxy;
}

Transport::~Transport() {
  VLOG(1) << "curl::Transport destroyed";
}

std::shared_ptr<http::Connection> Transport::CreateConnection(
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
    chromeos::Error::AddTo(error, FROM_HERE, http::kErrorDomain,
                           "curl_init_failed", "Failed to initialize CURL");
    return std::shared_ptr<http::Connection>();
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
  if (!proxy_.empty()) {
    curl_easy_setopt(curl_handle,
                     CURLOPT_PROXY, proxy_.c_str());
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

  std::shared_ptr<http::Connection> connection =
      std::make_shared<http::curl::Connection>(curl_handle, method, transport);
  if (!connection->SendHeaders(headers, error)) {
    connection.reset();
  }
  return connection;
}

void Transport::RunCallbackAsync(const tracked_objects::Location& from_here,
                                 const base::Closure& callback) {
  task_runner_->PostTask(from_here, callback);
}

void Transport::StartAsyncTransfer(http::Connection* connection,
                                   const SuccessCallback& success_callback,
                                   const ErrorCallback& error_callback) {
  // TODO(avakulenko): For now using synchronous operation behind the scenes,
  // but this is to change in the follow-up CLs.
  http::curl::Connection* curl_connection =
      static_cast<http::curl::Connection*>(connection);
  CURLcode ret = curl_easy_perform(curl_connection->curl_handle_);
  if (ret != CURLE_OK) {
    chromeos::ErrorPtr error;
    chromeos::Error::AddTo(&error, FROM_HERE, "curl_error",
                           chromeos::string_utils::ToString(ret),
                           curl_easy_strerror(ret));
    RunCallbackAsync(FROM_HERE,
                     base::Bind(error_callback, base::Owned(error.release())));
  } else {
    scoped_ptr<Response> response{new Response{connection->shared_from_this()}};
    RunCallbackAsync(FROM_HERE,
                     base::Bind(success_callback, base::Passed(&response)));
  }
}

}  // namespace curl
}  // namespace http
}  // namespace chromeos

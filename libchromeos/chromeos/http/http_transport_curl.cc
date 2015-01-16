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

Transport::Transport(const std::shared_ptr<CurlInterface>& curl_interface)
    : curl_interface_{curl_interface},
      task_runner_{base::MessageLoopProxy::current()} {
  VLOG(1) << "curl::Transport created";
}

Transport::Transport(const std::shared_ptr<CurlInterface>& curl_interface,
                     scoped_refptr<base::TaskRunner> task_runner)
    : curl_interface_{curl_interface}, task_runner_{task_runner} {
  VLOG(1) << "curl::Transport created";
}

Transport::Transport(const std::shared_ptr<CurlInterface>& curl_interface,
                     const std::string& proxy)
    : curl_interface_{curl_interface},
      proxy_{proxy},
      task_runner_{base::MessageLoopProxy::current()} {
  VLOG(1) << "curl::Transport created with proxy " << proxy;
}

Transport::~Transport() {
  VLOG(1) << "curl::Transport destroyed";
}

std::shared_ptr<http::Connection> Transport::CreateConnection(
    const std::string& url,
    const std::string& method,
    const HeaderList& headers,
    const std::string& user_agent,
    const std::string& referer,
    chromeos::ErrorPtr* error) {
  std::shared_ptr<http::Connection> connection;
  CURL* curl_handle = curl_interface_->EasyInit();
  if (!curl_handle) {
    LOG(ERROR) << "Failed to initialize CURL";
    chromeos::Error::AddTo(error,
                           FROM_HERE,
                           http::kErrorDomain,
                           "curl_init_failed",
                           "Failed to initialize CURL");
    return connection;
  }

  LOG(INFO) << "Sending a " << method << " request to " << url;
  CURLcode code = curl_interface_->EasySetOptStr(curl_handle, CURLOPT_URL, url);

  if (code == CURLE_OK && !user_agent.empty()) {
    code = curl_interface_->EasySetOptStr(
        curl_handle, CURLOPT_USERAGENT, user_agent);
  }
  if (code == CURLE_OK && !referer.empty()) {
    code =
        curl_interface_->EasySetOptStr(curl_handle, CURLOPT_REFERER, referer);
  }
  if (code == CURLE_OK && !proxy_.empty()) {
    code = curl_interface_->EasySetOptStr(curl_handle, CURLOPT_PROXY, proxy_);
  }

  // Setup HTTP request method and optional request body.
  if (code == CURLE_OK) {
    if (method == request_type::kGet) {
      code = curl_interface_->EasySetOptInt(curl_handle, CURLOPT_HTTPGET, 1);
    } else if (method == request_type::kHead) {
      code = curl_interface_->EasySetOptInt(curl_handle, CURLOPT_NOBODY, 1);
    } else if (method == request_type::kPut) {
      code = curl_interface_->EasySetOptInt(curl_handle, CURLOPT_UPLOAD, 1);
    } else {
      // POST and custom request methods
      code = curl_interface_->EasySetOptInt(curl_handle, CURLOPT_POST, 1);
      if (code == CURLE_OK) {
        code = curl_interface_->EasySetOptPtr(
            curl_handle, CURLOPT_POSTFIELDS, nullptr);
      }
      if (code == CURLE_OK && method != request_type::kPost) {
        code = curl_interface_->EasySetOptStr(
            curl_handle, CURLOPT_CUSTOMREQUEST, method);
      }
    }
  }

  if (code != CURLE_OK) {
    AddCurlError(error, FROM_HERE, code, curl_interface_.get());
    curl_interface_->EasyCleanup(curl_handle);
    return connection;
  }

  connection = std::make_shared<http::curl::Connection>(
      curl_handle, method, curl_interface_, shared_from_this());
  if (!connection->SendHeaders(headers, error)) {
    connection.reset();
  }
  return connection;
}

void Transport::RunCallbackAsync(const tracked_objects::Location& from_here,
                                 const base::Closure& callback) {
  task_runner_->PostTask(from_here, callback);
}

int Transport::StartAsyncTransfer(http::Connection* connection,
                                  const SuccessCallback& success_callback,
                                  const ErrorCallback& error_callback) {
  // TODO(avakulenko): For now using synchronous operation behind the scenes,
  // but this is to change in the follow-up CLs.
  http::curl::Connection* curl_connection =
      static_cast<http::curl::Connection*>(connection);
  CURLcode ret = curl_interface_->EasyPerform(curl_connection->curl_handle_);
  if (ret != CURLE_OK) {
    chromeos::ErrorPtr error;
    AddCurlError(&error, FROM_HERE, ret, curl_interface_.get());
    RunCallbackAsync(FROM_HERE, base::Bind(error_callback,
                                           0, base::Owned(error.release())));
  } else {
    scoped_ptr<Response> response{new Response{connection->shared_from_this()}};
    RunCallbackAsync(FROM_HERE,
                     base::Bind(success_callback, 1, base::Passed(&response)));
  }
  return 1;
}

bool Transport::CancelRequest(int request_id) {
  // TODO(avakulenko): Implementation of this will come later.
  return false;
}

void Transport::AddCurlError(chromeos::ErrorPtr* error,
                             const tracked_objects::Location& location,
                             CURLcode code,
                             CurlInterface* curl_interface) {
  chromeos::Error::AddTo(error,
                         location,
                         "curl_error",
                         chromeos::string_utils::ToString(code),
                         curl_interface->EasyStrError(code));
}

}  // namespace curl
}  // namespace http
}  // namespace chromeos

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_EXAMPLES_UBUNTU_CURL_HTTP_CLIENT_H_
#define LIBWEAVE_EXAMPLES_UBUNTU_CURL_HTTP_CLIENT_H_

#include <string>

#include <base/memory/weak_ptr.h>
#include <weave/http_client.h>

namespace weave {

class TaskRunner;

namespace examples {

// Basic implementation of weave::HttpClient using libcurl. Should be used in
// production code as it's blocking and does not validate server certificates.
class CurlHttpClient : public HttpClient {
 public:
  explicit CurlHttpClient(TaskRunner* task_runner);

  std::unique_ptr<Response> SendRequestAndBlock(const std::string& method,
                                                const std::string& url,
                                                const Headers& headers,
                                                const std::string& data,
                                                ErrorPtr* error) override;
  int SendRequest(const std::string& method,
                  const std::string& url,
                  const Headers& headers,
                  const std::string& data,
                  const SuccessCallback& success_callback,
                  const ErrorCallback& error_callback) override;

 private:
  void RunSuccessCallback(const SuccessCallback& success_callback,
                          int id,
                          std::unique_ptr<Response> response);
  void RunErrorCallback(const ErrorCallback& error_callback,
                        int id,
                        ErrorPtr error);

  TaskRunner* task_runner_{nullptr};
  int request_id_ = 0;

  base::WeakPtrFactory<CurlHttpClient> weak_ptr_factory_{this};
};

}  // namespace examples
}  // namespace weave

#endif  // LIBWEAVE_EXAMPLES_UBUNTU_CURL_HTTP_CLIENT_H_

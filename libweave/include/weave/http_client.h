// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_HTTP_CLIENT_H_
#define LIBWEAVE_INCLUDE_WEAVE_HTTP_CLIENT_H_

#include <string>
#include <utility>
#include <vector>

#include <base/callback.h>
#include <chromeos/errors/error.h>

namespace weave {

class HttpClient {
 public:
  class Response {
   public:
    virtual int GetStatusCode() const = 0;
    virtual std::string GetContentType() const = 0;
    virtual const std::string& GetData() const = 0;

    // TODO(vitalybuka): Hide when SendRequestAndBlock is removed.
    virtual ~Response() = default;
  };

  using Headers = std::vector<std::pair<std::string, std::string>>;
  using SuccessCallback = base::Callback<void(int, const Response&)>;
  using ErrorCallback = base::Callback<void(int, const chromeos::Error*)>;

  // TODO(vitalybuka): Remove blocking version.
  virtual std::unique_ptr<Response> SendRequestAndBlock(
      const std::string& method,
      const std::string& url,
      const std::string& data,
      const std::string& mime_type,
      const Headers& headers,
      chromeos::ErrorPtr* error) = 0;

  virtual int SendRequest(const std::string& method,
                          const std::string& url,
                          const std::string& data,
                          const std::string& mime_type,
                          const Headers& headers,
                          const SuccessCallback& success_callback,
                          const ErrorCallback& error_callback) = 0;

 protected:
  virtual ~HttpClient() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_HTTP_CLIENT_H_

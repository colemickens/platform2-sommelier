// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_HTTP_SERVER_H_
#define LIBWEAVE_INCLUDE_WEAVE_HTTP_SERVER_H_

#include <string>
#include <vector>

#include <chromeos/secure_blob.h>

#include <base/callback.h>

namespace weave {

class HttpServer {
 public:
  class Request {
   public:
    virtual const std::string& GetPath() const = 0;
    virtual std::string GetFirstHeader(const std::string& name) const = 0;
    virtual const std::vector<uint8_t>& GetData() const = 0;

   protected:
    virtual ~Request() = default;
  };

  using OnStateChangedCallback = base::Callback<void(const HttpServer& server)>;

  using OnReplyCallback = base::Callback<void(int status_code,
                                              const std::string& data,
                                              const std::string& mime_type)>;

  using OnRequestCallback =
      base::Callback<void(const Request& request,
                          const OnReplyCallback& callback)>;

  // Adds notification callback for server started/stopped serving requests.
  virtual void AddOnStateChangedCallback(
      const OnStateChangedCallback& callback) = 0;

  // Adds callback called on new http/https requests with the given path prefix.
  virtual void AddRequestHandler(const std::string& path_prefix,
                                 const OnRequestCallback& callback) = 0;

  virtual uint16_t GetHttpPort() const = 0;
  virtual uint16_t GetHttpsPort() const = 0;
  virtual const chromeos::Blob& GetHttpsCertificateFingerprint() const = 0;

 protected:
  virtual ~HttpServer() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_HTTP_SERVER_H_

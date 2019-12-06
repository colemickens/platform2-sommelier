// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_WEBSERVD_REQUEST_HANDLER_INTERFACE_H_
#define WEBSERVER_WEBSERVD_REQUEST_HANDLER_INTERFACE_H_

#include <string>

#include <base/macros.h>

namespace webservd {

class Request;

// An abstract interface to dispatch of HTTP requests to remote handlers over
// implementation-specific IPC (e.g. D-Bus).
class RequestHandlerInterface {
 public:
  RequestHandlerInterface() = default;
  virtual ~RequestHandlerInterface() = default;

  virtual void HandleRequest(Request* request, const std::string& src) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RequestHandlerInterface);
};

}  // namespace webservd

#endif  // WEBSERVER_WEBSERVD_REQUEST_HANDLER_INTERFACE_H_

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_REQUEST_HANDLER_INTERFACE_H_
#define LIBWEBSERV_REQUEST_HANDLER_INTERFACE_H_

#include <memory>

namespace libwebserv {

class Request;
class Response;
using RequestPtr = std::shared_ptr<const Request>;
using ResponsePtr = std::shared_ptr<Response>;

// The base interface for HTTP request handlers. When registering a handler,
// the RequestHandlerInterface is provided, and when a request comes in,
// RequestHandlerInterface::HandleRequest() is called to process the data and
// send response.
class RequestHandlerInterface {
 public:
  using HandlerSignature = void(const std::shared_ptr<const Request>&,
                                const std::shared_ptr<Response>&);

  virtual void HandleRequest(const std::shared_ptr<const Request>& request,
                             const std::shared_ptr<Response>& response) = 0;
};

}  // namespace libwebserv

#endif  // LIBWEBSERV_REQUEST_HANDLER_INTERFACE_H_

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEBSERV_REQUEST_HANDLER_INTERFACE_H_
#define LIBWEBSERV_REQUEST_HANDLER_INTERFACE_H_

#include <base/memory/scoped_ptr.h>

#include "libwebserv/request.h"
#include "libwebserv/response.h"

namespace libwebserv {

// The base interface for HTTP request handlers. When registering a handler,
// the RequestHandlerInterface is provided, and when a request comes in,
// RequestHandlerInterface::HandleRequest() is called to process the data and
// send response.
class RequestHandlerInterface {
 public:
  using HandlerSignature = void(scoped_ptr<Request>, scoped_ptr<Response>);

  virtual void HandleRequest(scoped_ptr<Request> request,
                             scoped_ptr<Response> response) = 0;
};

}  // namespace libwebserv

#endif  // LIBWEBSERV_REQUEST_HANDLER_INTERFACE_H_

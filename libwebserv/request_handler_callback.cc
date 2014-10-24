// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libwebserv/request_handler_callback.h"

namespace libwebserv {

RequestHandlerCallback::RequestHandlerCallback(
    const base::Callback<HandlerSignature>& callback) : callback_(callback) {
}

void RequestHandlerCallback::HandleRequest(
    const std::shared_ptr<const Request>& request,
    const std::shared_ptr<Response>& response) {
  callback_.Run(request, response);
}

}  // namespace libwebserv

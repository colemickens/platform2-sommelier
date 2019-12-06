// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBSERVER_LIBWEBSERV_REQUEST_UTILS_H_
#define WEBSERVER_LIBWEBSERV_REQUEST_UTILS_H_

#include <memory>
#include <vector>

#include <base/callback_forward.h>
#include <brillo/errors/error.h>
#include <libwebserv/export.h>

namespace libwebserv {

class Request;
class Response;

using GetRequestDataSuccessCallback =
    base::Callback<void(std::unique_ptr<Request> request,
                        std::unique_ptr<Response> response,
                        std::vector<uint8_t> data)>;

using GetRequestDataErrorCallback =
    base::Callback<void(std::unique_ptr<Request> request,
                        std::unique_ptr<Response> response,
                        const brillo::Error* error)>;

// Reads the request data from |request| asynchronously and returns the data
// by calling |success_callback|. If an error occurred, |error_callback| is
// invoked with the error information passed into |error| parameter.
// The function takes ownership of the request and response objects for the
// duration of operation and returns them back via the callbacks.
LIBWEBSERV_EXPORT void GetRequestData(
    std::unique_ptr<Request> request,
    std::unique_ptr<Response> response,
    const GetRequestDataSuccessCallback& success_callback,
    const GetRequestDataErrorCallback& error_callback);

}  // namespace libwebserv

#endif  // WEBSERVER_LIBWEBSERV_REQUEST_UTILS_H_

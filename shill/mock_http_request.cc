// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_http_request.h"

#include "shill/connection.h"

namespace shill {

MockHttpRequest::MockHttpRequest(ConnectionRefPtr connection)
    : HttpRequest(connection, nullptr, true) {}

MockHttpRequest::~MockHttpRequest() = default;

}  // namespace shill

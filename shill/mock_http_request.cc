// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_http_request.h"

#include "shill/connection.h"

namespace shill {

MockHTTPRequest::MockHTTPRequest(ConnectionRefPtr connection)
    : HTTPRequest(connection,
                  reinterpret_cast<EventDispatcher *>(NULL),
                  reinterpret_cast<Sockets *>(NULL)) {}

MockHTTPRequest::~MockHTTPRequest() {}

}  // namespace shill

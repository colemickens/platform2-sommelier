// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_http_request.h"

namespace {
constexpr const char kMockInterfaceName[] = "mock0";
}  // namespace

namespace shill {

MockHttpRequest::MockHttpRequest()
    : HttpRequest(
          nullptr, kMockInterfaceName, IPAddress::kFamilyIPv4, {}, true) {}

MockHttpRequest::~MockHttpRequest() = default;

}  // namespace shill

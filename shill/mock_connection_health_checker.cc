// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_connection_health_checker.h"

#include "shill/async_connection.h"
#include "shill/connection.h"

using base::Callback;

namespace shill {

MockConnectionHealthChecker::MockConnectionHealthChecker(
    ConnectionRefPtr connection,
    EventDispatcher* dispatcher,
    IPAddressStore* remote_ips,
    const Callback<void(Result)>& result_callback)
    : ConnectionHealthChecker(connection,
                              dispatcher,
                              remote_ips,
                              result_callback) {}

MockConnectionHealthChecker::~MockConnectionHealthChecker() {}

}  // namespace shill

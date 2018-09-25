// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_async_connection.h"

#include "shill/net/ip_address.h"

namespace shill {

MockAsyncConnection::MockAsyncConnection()
    : AsyncConnection("", nullptr, nullptr, base::Callback<void(bool, int)>()) {
}

MockAsyncConnection::~MockAsyncConnection() {}

}  // namespace shill

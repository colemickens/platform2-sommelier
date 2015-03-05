// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "firewalld/mock_iptables.h"

namespace firewalld {

MockIpTables::MockIpTables() : IpTables("", "") {}

MockIpTables::~MockIpTables() {}

}  // namespace firewalld

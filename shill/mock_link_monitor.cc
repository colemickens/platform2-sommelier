// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_link_monitor.h"

#include "shill/connection.h"

namespace shill {

MockLinkMonitor::MockLinkMonitor()
    : LinkMonitor(NULL, NULL, NULL, FailureCallback()) {}

MockLinkMonitor::~MockLinkMonitor() {}

}  // namespace shill

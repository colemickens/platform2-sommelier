// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_active_link_monitor.h"

#include "shill/connection.h"

namespace shill {

MockActiveLinkMonitor::MockActiveLinkMonitor()
    : ActiveLinkMonitor(nullptr, nullptr, nullptr, nullptr, FailureCallback(),
                        SuccessCallback()) {}

MockActiveLinkMonitor::~MockActiveLinkMonitor() = default;

}  // namespace shill

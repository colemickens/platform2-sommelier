// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_portal_detector.h"

#include "shill/connection.h"

namespace shill {

MockPortalDetector::MockPortalDetector(ConnectionRefPtr connection)
    : PortalDetector(connection, NULL, base::Callback<void(const Result&)>()) {}

MockPortalDetector::~MockPortalDetector() {}

}  // namespace shill

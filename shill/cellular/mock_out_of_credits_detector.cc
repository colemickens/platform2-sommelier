// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/mock_out_of_credits_detector.h"

#include <gmock/gmock.h>

namespace shill {

MockOutOfCreditsDetector::MockOutOfCreditsDetector(
    EventDispatcher* dispatcher,
    Manager* manager,
    Metrics* metrics,
    CellularService* service)
    : OutOfCreditsDetector(dispatcher, manager, metrics, service) {}

MockOutOfCreditsDetector::~MockOutOfCreditsDetector() {}

}  // namespace shill

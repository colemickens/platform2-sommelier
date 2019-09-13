// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PORTAL_DETECTOR_H_
#define SHILL_MOCK_PORTAL_DETECTOR_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/portal_detector.h"

namespace shill {

class MockPortalDetector : public PortalDetector {
 public:
  explicit MockPortalDetector(ConnectionRefPtr connection);
  ~MockPortalDetector() override;

  MOCK_METHOD(bool,
              StartAfterDelay,
              (const PortalDetector::Properties&, int),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(bool, IsInProgress, (), (override));
  MOCK_METHOD(int, AdjustStartDelay, (int), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPortalDetector);
};

}  // namespace shill

#endif  // SHILL_MOCK_PORTAL_DETECTOR_H_

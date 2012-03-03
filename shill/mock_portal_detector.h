// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PORTAL_DETECTOR_H_
#define SHILL_MOCK_PORTAL_DETECTOR_H_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/portal_detector.h"

namespace shill {

class MockPortalDetector : public PortalDetector {
 public:
  MockPortalDetector(ConnectionRefPtr connection);
  virtual ~MockPortalDetector();

  MOCK_METHOD1(Start, bool(const std::string &));
  MOCK_METHOD2(StartAfterDelay, bool(const std::string &, int delay_seconds));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(IsInProgress, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPortalDetector);
};

}  // namespace shill

#endif  // SHILL_MOCK_PORTAL_DETECTOR_H_

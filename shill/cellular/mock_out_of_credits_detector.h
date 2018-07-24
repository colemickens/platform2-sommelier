// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_MOCK_OUT_OF_CREDITS_DETECTOR_H_
#define SHILL_CELLULAR_MOCK_OUT_OF_CREDITS_DETECTOR_H_

#include <gmock/gmock.h>

#include "shill/cellular/out_of_credits_detector.h"

namespace shill {

class MockOutOfCreditsDetector : public OutOfCreditsDetector {
 public:
  MockOutOfCreditsDetector(EventDispatcher* dispatcher,
                           Manager* manager,
                           Metrics* metrics,
                           CellularService* service);
  ~MockOutOfCreditsDetector() override;

  MOCK_METHOD0(ResetDetector, void());
  MOCK_CONST_METHOD0(IsDetecting, bool());
  MOCK_METHOD2(NotifyServiceStateChanged,
               void(Service::ConnectState old_state,
                    Service::ConnectState new_state));
  MOCK_METHOD1(NotifySubscriptionStateChanged,
               void(uint32_t subscription_state));
  MOCK_CONST_METHOD0(out_of_credits, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockOutOfCreditsDetector);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_MOCK_OUT_OF_CREDITS_DETECTOR_H_

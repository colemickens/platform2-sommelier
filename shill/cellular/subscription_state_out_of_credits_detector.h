// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_SUBSCRIPTION_STATE_OUT_OF_CREDITS_DETECTOR_H_
#define SHILL_CELLULAR_SUBSCRIPTION_STATE_OUT_OF_CREDITS_DETECTOR_H_

#include "shill/cellular/out_of_credits_detector.h"

namespace shill {

// Detects out-of-credits condition by using the subscription state.
class SubscriptionStateOutOfCreditsDetector : public OutOfCreditsDetector {
 public:
  SubscriptionStateOutOfCreditsDetector(EventDispatcher* dispatcher,
                                        Manager* manager,
                                        Metrics* metrics,
                                        CellularService* service);
  ~SubscriptionStateOutOfCreditsDetector() override;

  void ResetDetector() override {}
  bool IsDetecting() const override { return false; }
  void NotifyServiceStateChanged(
      Service::ConnectState old_state,
      Service::ConnectState new_state) override {}
  void NotifySubscriptionStateChanged(uint32_t subscription_state) override;

 private:
  friend class SubscriptionStateOutOfCreditsDetectorTest;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionStateOutOfCreditsDetector);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_SUBSCRIPTION_STATE_OUT_OF_CREDITS_DETECTOR_H_

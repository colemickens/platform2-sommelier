// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUBSCRIPTION_STATE_OUT_OF_CREDITS_DETECTOR_H_
#define SHILL_SUBSCRIPTION_STATE_OUT_OF_CREDITS_DETECTOR_H_

#include "shill/out_of_credits_detector.h"

namespace shill {

// Detects out-of-credits condition by using the subscription state.
class SubscriptionStateOutOfCreditsDetector : public OutOfCreditsDetector {
 public:
  SubscriptionStateOutOfCreditsDetector(EventDispatcher *dispatcher,
                                        Manager *manager,
                                        Metrics *metrics,
                                        CellularService *service);
  virtual ~SubscriptionStateOutOfCreditsDetector();

  virtual void ResetDetector() override {}
  virtual bool IsDetecting() const override { return false; }
  virtual void NotifyServiceStateChanged(
      Service::ConnectState old_state,
      Service::ConnectState new_state) override {}
  virtual void NotifySubscriptionStateChanged(
      uint32_t subscription_state) override;

 private:
  friend class SubscriptionStateOutOfCreditsDetectorTest;

  DISALLOW_COPY_AND_ASSIGN(SubscriptionStateOutOfCreditsDetector);
};

}  // namespace shill

#endif  // SHILL_SUBSCRIPTION_STATE_OUT_OF_CREDITS_DETECTOR_H_

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/subscription_state_out_of_credits_detector.h"

#include "ModemManager/ModemManager.h"

#include "shill/logging.h"

namespace shill {

SubscriptionStateOutOfCreditsDetector::SubscriptionStateOutOfCreditsDetector(
    EventDispatcher *dispatcher,
    Manager *manager,
    Metrics *metrics,
    CellularService *service)
    : OutOfCreditsDetector(dispatcher, manager, metrics, service) {
}

SubscriptionStateOutOfCreditsDetector::
    ~SubscriptionStateOutOfCreditsDetector() {
}

void SubscriptionStateOutOfCreditsDetector::NotifySubscriptionStateChanged(
    uint32 subscription_state) {
  bool ooc = (static_cast<MMModem3gppSubscriptionState>(subscription_state) ==
              MM_MODEM_3GPP_SUBSCRIPTION_STATE_OUT_OF_DATA);
  if (ooc != out_of_credits()) {
    if (ooc)
      SLOG(Cellular, 2) << "Marking service out-of-credits";
    else
      SLOG(Cellular, 2) << "Marking service as not out-of-credits";
  }
  ReportOutOfCredits(ooc);
}

}  // namespace shill

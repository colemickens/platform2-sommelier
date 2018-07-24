// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular/subscription_state_out_of_credits_detector.h"

#include <string>

#include "ModemManager/ModemManager.h"

#include "shill/cellular/cellular_service.h"
#include "shill/logging.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kCellular;
static string ObjectID(CellularService* c) { return c->GetRpcIdentifier(); }
}

SubscriptionStateOutOfCreditsDetector::SubscriptionStateOutOfCreditsDetector(
    EventDispatcher* dispatcher,
    Manager* manager,
    Metrics* metrics,
    CellularService* service)
    : OutOfCreditsDetector(dispatcher, manager, metrics, service) {
}

SubscriptionStateOutOfCreditsDetector::
    ~SubscriptionStateOutOfCreditsDetector() {
}

void SubscriptionStateOutOfCreditsDetector::NotifySubscriptionStateChanged(
    uint32_t subscription_state) {
  bool ooc = (static_cast<MMModem3gppSubscriptionState>(subscription_state) ==
              MM_MODEM_3GPP_SUBSCRIPTION_STATE_OUT_OF_DATA);
  if (ooc != out_of_credits()) {
    if (ooc)
      SLOG(service(), 2) << "Marking service out-of-credits";
    else
      SLOG(service(), 2) << "Marking service as not out-of-credits";
  }
  ReportOutOfCredits(ooc);
}

}  // namespace shill

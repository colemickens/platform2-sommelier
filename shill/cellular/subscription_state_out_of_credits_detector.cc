//
// Copyright (C) 2013 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

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
    CellularService* service)
    : OutOfCreditsDetector(service) {
}

SubscriptionStateOutOfCreditsDetector::
    ~SubscriptionStateOutOfCreditsDetector() {
}

void SubscriptionStateOutOfCreditsDetector::NotifySubscriptionStateChanged(
    SubscriptionState subscription_state) {
  bool ooc = (subscription_state == SubscriptionState::kOutOfCredits);
  if (ooc != out_of_credits()) {
    if (ooc)
      SLOG(service(), 2) << "Marking service out-of-credits";
    else
      SLOG(service(), 2) << "Marking service as not out-of-credits";
  }
  ReportOutOfCredits(ooc);
}

}  // namespace shill

//
// Copyright 2018 The Chromium OS Authors. All rights reserved.
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

#ifndef SHILL_CELLULAR_SUBSCRIPTION_STATE_H_
#define SHILL_CELLULAR_SUBSCRIPTION_STATE_H_

#include <string>

namespace shill {

// CellularSubscriptionState represents the provisioned state of SIM. It is used
// currently by activation logic for LTE to determine if activation process is
// complete.
enum class SubscriptionState {
  kUnknown,
  kUnprovisioned,
  kProvisioned,
  kOutOfCredits,
};

std::string SubscriptionStateToString(SubscriptionState subscription_state);

}  // namespace shill

#endif  // SHILL_CELLULAR_SUBSCRIPTION_STATE_H_

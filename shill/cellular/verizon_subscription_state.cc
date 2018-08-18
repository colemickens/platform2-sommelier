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

#include "shill/cellular/verizon_subscription_state.h"

#include <vector>

namespace shill {

bool FindVerizonSubscriptionStateFromPco(
    const CellularPco& pco, SubscriptionState* subscription_state) {
  // Expected format:
  //       ID: FF 00
  //   Length: 04
  //     Data: 13 01 84 <x>
  //
  // where <x> can be:
  //    00: provisioned
  //    03: out of data credits
  //    05: unprovisioned

  const CellularPco::Element* element = pco.FindElement(0xFF00);
  if (!element)
    return false;

  const std::vector<uint8_t>& pco_data = element->data;
  if (pco_data.size() != 4 || pco_data[0] != 0x13 || pco_data[1] != 0x01 ||
      pco_data[2] != 0x84) {
    return false;
  }

  switch (pco_data[3]) {
    case 0x00:
      *subscription_state = SubscriptionState::kProvisioned;
      break;
    case 0x03:
      *subscription_state = SubscriptionState::kOutOfCredits;
      break;
    case 0x05:
      *subscription_state = SubscriptionState::kUnprovisioned;
      break;
    default:
      *subscription_state = SubscriptionState::kUnknown;
      break;
  }
  return true;
}

}  // namespace shill

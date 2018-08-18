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

#ifndef SHILL_CELLULAR_VERIZON_SUBSCRIPTION_STATE_H_
#define SHILL_CELLULAR_VERIZON_SUBSCRIPTION_STATE_H_

#include "shill/cellular/cellular_pco.h"
#include "shill/cellular/subscription_state.h"

namespace shill {

// Finds the Verizon subscription state from the specified PCO. Returns true if
// the PCO contains a Verizon-specific PCO value and |subscription_state| is
// set according to the PCO value. Returns false if no Verizon-specific PCO is
// found.
bool FindVerizonSubscriptionStateFromPco(const CellularPco& pco,
                                         SubscriptionState* subscription_state);

}  // namespace shill

#endif  // SHILL_CELLULAR_VERIZON_SUBSCRIPTION_STATE_H_

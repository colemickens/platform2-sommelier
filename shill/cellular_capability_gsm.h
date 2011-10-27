// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_GSM_
#define SHILL_CELLULAR_CAPABILITY_GSM_

#include "shill/cellular_capability.h"

namespace shill {

class CellularCapabilityGSM : public CellularCapability {
 public:
  CellularCapabilityGSM(Cellular *cellular);

  // Inherited from CellularCapability;
  virtual void InitProxies();

 private:
  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityGSM);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_GSM_

// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_CELLULAR_CAPABILITY_CDMA_
#define SHILL_CELLULAR_CAPABILITY_CDMA_

#include "shill/cellular_capability.h"

namespace shill {

class CellularCapabilityCDMA : public CellularCapability {
 public:
  CellularCapabilityCDMA(Cellular *cellular);

  // Inherited from CellularCapability.
  virtual void InitProxies();

  // Obtains the MEID.
  virtual void GetIdentifiers();

 private:
  DISALLOW_COPY_AND_ASSIGN(CellularCapabilityCDMA);
};

}  // namespace shill

#endif  // SHILL_CELLULAR_CAPABILITY_CDMA_

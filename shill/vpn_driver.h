// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_DRIVER_
#define SHILL_VPN_DRIVER_

#include <string>

#include <base/basictypes.h>

#include "shill/refptr_types.h"

namespace shill {

class Error;

class VPNDriver {
 public:
  virtual ~VPNDriver() {}

  virtual bool ClaimInterface(const std::string &link_name,
                              int interface_index) = 0;
  virtual void Connect(const VPNServiceRefPtr &service, Error *error) = 0;
  virtual void Disconnect() = 0;
};

}  // namespace shill

#endif  // SHILL_VPN_DRIVER_

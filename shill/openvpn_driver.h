// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_DRIVER_
#define SHILL_OPENVPN_DRIVER_

#include "shill/vpn_driver.h"

namespace shill {

class Error;

class OpenVPNDriver : public VPNDriver {
 public:
  OpenVPNDriver();
  virtual ~OpenVPNDriver();

  // Inherited from VPNDriver.
  virtual void Connect(Error *error);

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenVPNDriver);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_DRIVER_

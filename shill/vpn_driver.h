// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_DRIVER_
#define SHILL_VPN_DRIVER_

#include <base/basictypes.h>

namespace shill {

class Error;

class VPNDriver {
 public:
  virtual ~VPNDriver() {}

  virtual void Connect(Error *error) = 0;
};

}  // namespace shill

#endif  // SHILL_VPN_DRIVER_

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_driver.h"

#include "shill/error.h"

namespace shill {

OpenVPNDriver::OpenVPNDriver() {}

OpenVPNDriver::~OpenVPNDriver() {}

void OpenVPNDriver::Connect(Error *error) {
  error->Populate(Error::kNotSupported);
}

}  // namespace shill

// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_supplicant_interface_proxy.h"

#include "shill/wifi.h"

namespace shill {

MockSupplicantInterfaceProxy::MockSupplicantInterfaceProxy(
    const WiFiRefPtr &wifi) : wifi_(wifi) {}

MockSupplicantInterfaceProxy::~MockSupplicantInterfaceProxy() {}

}  // namespace shill

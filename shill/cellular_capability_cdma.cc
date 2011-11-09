// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <base/logging.h>

#include "shill/cellular.h"
#include "shill/proxy_factory.h"

namespace shill {

CellularCapabilityCDMA::CellularCapabilityCDMA(Cellular *cellular)
    : CellularCapability(cellular) {}

void CellularCapabilityCDMA::InitProxies() {
  VLOG(2) << __func__;
  // TODO(petkov): Move CDMA-specific proxy ownership from Cellular to this.
  cellular()->set_modem_cdma_proxy(
      proxy_factory()->CreateModemCDMAProxy(cellular(),
                                            cellular()->dbus_path(),
                                            cellular()->dbus_owner()));
}

void CellularCapabilityCDMA::GetIdentifiers() {
  VLOG(2) << __func__;
  if (cellular()->meid().empty()) {
    // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
    cellular()->set_meid(cellular()->modem_cdma_proxy()->MEID());
    VLOG(2) << "MEID: " << cellular()->imei();
  }
}

}  // namespace shill

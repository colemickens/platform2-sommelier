// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_gsm.h"

#include <base/logging.h>

#include "shill/cellular.h"
#include "shill/proxy_factory.h"

namespace shill {

CellularCapabilityGSM::CellularCapabilityGSM(Cellular *cellular)
    : CellularCapability(cellular) {}

void CellularCapabilityGSM::InitProxies() {
  VLOG(2) << __func__;
  // TODO(petkov): Move GSM-specific proxy ownership from Cellular to this.
  cellular()->set_modem_gsm_card_proxy(
      proxy_factory()->CreateModemGSMCardProxy(cellular(),
                                               cellular()->dbus_path(),
                                               cellular()->dbus_owner()));
  cellular()->set_modem_gsm_network_proxy(
      proxy_factory()->CreateModemGSMNetworkProxy(cellular(),
                                                  cellular()->dbus_path(),
                                                  cellular()->dbus_owner()));
}

}  // namespace shill

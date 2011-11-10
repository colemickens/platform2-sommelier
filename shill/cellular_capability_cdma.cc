// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability_cdma.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>
#include <mm/mm-modem.h>

#include "shill/cellular.h"
#include "shill/proxy_factory.h"

using std::string;

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

string CellularCapabilityCDMA::GetNetworkTechnologyString() const {
  if (cellular()->cdma_registration_state_evdo() !=
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    return flimflam::kNetworkTechnologyEvdo;
  }
  if (cellular()->cdma_registration_state_1x() !=
      MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    return flimflam::kNetworkTechnology1Xrtt;
  }
  return "";
}

string CellularCapabilityCDMA::GetRoamingStateString() const {
  uint32 state = cellular()->cdma_registration_state_evdo();
  if (state == MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN) {
    state = cellular()->cdma_registration_state_1x();
  }
  switch (state) {
    case MM_MODEM_CDMA_REGISTRATION_STATE_UNKNOWN:
    case MM_MODEM_CDMA_REGISTRATION_STATE_REGISTERED:
      break;
    case MM_MODEM_CDMA_REGISTRATION_STATE_HOME:
      return flimflam::kRoamingStateHome;
    case MM_MODEM_CDMA_REGISTRATION_STATE_ROAMING:
      return flimflam::kRoamingStateRoaming;
    default:
      NOTREACHED();
  }
  return flimflam::kRoamingStateUnknown;
}

void CellularCapabilityCDMA::GetSignalQuality() {
  VLOG(2) << __func__;
  // TODO(petkov): Switch to asynchronous calls (crosbug.com/17583).
  uint32 strength = cellular()->modem_cdma_proxy()->GetSignalQuality();
  cellular()->HandleNewSignalQuality(strength);
}

}  // namespace shill

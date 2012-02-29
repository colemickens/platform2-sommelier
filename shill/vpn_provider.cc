// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_provider.h"

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/openvpn_driver.h"
#include "shill/vpn_service.h"

using std::string;

namespace shill {

VPNProvider::VPNProvider(ControlInterface *control_interface,
                         EventDispatcher *dispatcher,
                         Metrics *metrics,
                         Manager *manager)
    : control_interface_(control_interface),
      dispatcher_(dispatcher),
      metrics_(metrics),
      manager_(manager) {}

VPNProvider::~VPNProvider() {}

void VPNProvider::Start() {}

void VPNProvider::Stop() {}

VPNServiceRefPtr VPNProvider::GetService(const KeyValueStore &args,
                                         Error *error) {
  VLOG(2) << __func__;
  if (!args.ContainsString(flimflam::kProviderTypeProperty)) {
    Error::PopulateAndLog(
        error, Error::kNotSupported, "Missing VPN type property.");
    return NULL;
  }
  const string &type = args.GetString(flimflam::kProviderTypeProperty);
  scoped_ptr<VPNDriver> driver;
  if (type == flimflam::kProviderOpenVpn) {
    driver.reset(new OpenVPNDriver(control_interface_, args));
  } else {
    Error::PopulateAndLog(
        error, Error::kNotSupported, "Unsupported VPN type: " + type);
    return NULL;
  }
  return new VPNService(
      control_interface_, dispatcher_, metrics_, manager_, driver.release());
}

}  // namespace shill

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_provider.h"

#include <algorithm>

#include <base/logging.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/manager.h"
#include "shill/openvpn_driver.h"
#include "shill/vpn_service.h"

using std::string;
using std::vector;

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
  string type = args.LookupString(flimflam::kProviderTypeProperty, "");
  if (type.empty()) {
    Error::PopulateAndLog(
        error, Error::kNotSupported, "Missing VPN type property.");
    return NULL;
  }

  string storage_id = VPNService::CreateStorageIdentifier(args, error);
  if (storage_id.empty()) {
    return NULL;
  }

  // Find a service in the provider list which matches these parameters.
  for (vector<VPNServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if (type == (*it)->driver()->GetProviderType() &&
        (*it)->GetStorageIdentifier() == storage_id) {
      return *it;
    }
  }

  scoped_ptr<VPNDriver> driver;
  if (type == flimflam::kProviderOpenVpn) {
    driver.reset(new OpenVPNDriver(
        control_interface_, dispatcher_, metrics_, manager_,
        manager_->device_info(), manager_->glib(), args));
  } else {
    Error::PopulateAndLog(
        error, Error::kNotSupported, "Unsupported VPN type: " + type);
    return NULL;
  }

  VPNServiceRefPtr service = new VPNService(
      control_interface_, dispatcher_, metrics_, manager_, driver.release());
  service->set_storage_id(storage_id);
  service->InitDriverPropertyStore();
  string name = args.LookupString(flimflam::kProviderNameProperty, "");
  if (!name.empty()) {
    service->set_friendly_name(name);
  }
  services_.push_back(service);
  manager_->RegisterService(service);
  return service;
}

bool VPNProvider::OnDeviceInfoAvailable(const string &link_name,
                                        int interface_index) {
  for (vector<VPNServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if ((*it)->driver()->ClaimInterface(link_name, interface_index)) {
      return true;
    }
  }

  return false;
}

void VPNProvider::RemoveService(VPNServiceRefPtr service) {
  vector<VPNServiceRefPtr>::iterator it;
  it = std::find(services_.begin(), services_.end(), service);
  if (it != services_.end()) {
    services_.erase(it);
  }
}

}  // namespace shill

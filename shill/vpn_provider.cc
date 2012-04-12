// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_provider.h"

#include <algorithm>

#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/error.h"
#include "shill/manager.h"
#include "shill/openvpn_driver.h"
#include "shill/profile.h"
#include "shill/store_interface.h"
#include "shill/vpn_service.h"

using std::set;
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
  VPNServiceRefPtr service = FindService(type, storage_id);

  if (service == NULL) {
    // Create a service, using the name and type arguments passed in.
    string name = args.LookupString(flimflam::kProviderNameProperty, "");
    if (name.empty()) {
      name = args.LookupString(flimflam::kNameProperty, "");
    }
    service = CreateService(type, name, storage_id, error);
  }

  if (service != NULL) {
    // Configure the service using the the rest of the passed-in arguments.
    service->Configure(args, error);
  }

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

void VPNProvider::CreateServicesFromProfile(ProfileRefPtr profile) {
  VLOG(2) << __func__;
  const StoreInterface *storage = profile->GetConstStorage();
  set<string> groups =
      storage->GetGroupsWithKey(flimflam::kProviderTypeProperty);
  for (set<string>::iterator it = groups.begin(); it != groups.end(); ++it) {
    if (!StartsWithASCII(*it, "vpn_", false)) {
      continue;
    }

    string type;
    if (!storage->GetString(*it, flimflam::kProviderTypeProperty, &type)) {
      LOG(ERROR) << "Group " << *it << " is missing the "
                 << flimflam::kProviderTypeProperty << " property.";
      continue;
    }

    string name;
    if (!storage->GetString(*it, flimflam::kProviderNameProperty, &name) &&
        !storage->GetString(*it, flimflam::kNameProperty, &name)) {
      LOG(ERROR) << "Group " << *it << " is missing the "
                 << flimflam::kProviderNameProperty << " property.";
      continue;
    }

    VPNServiceRefPtr service = FindService(type, *it);
    if (service != NULL) {
      // If the service already exists, it does not need to be configured,
      // since PushProfile would have already called ConfigureService on it.
      VLOG(2) << "Service already exists " << *it;
      continue;
    }

    Error error;
    service = CreateService(type, name, *it, &error);

    if (service == NULL) {
      LOG(ERROR) << "Could not create service for " << *it;
      continue;
    }

    if (!profile->ConfigureService(service)) {
      LOG(ERROR) << "Could not configure service for " << *it;
      continue;
    }
  }
}

VPNServiceRefPtr VPNProvider::CreateService(const string &type,
                                            const string &name,
                                            const string &storage_id,
                                            Error *error) {
  VLOG(2) << __func__ << " type " << type << " name " << name
          << " storage id " << storage_id;
  scoped_ptr<VPNDriver> driver;
  if (type == flimflam::kProviderOpenVpn) {
    driver.reset(new OpenVPNDriver(
        control_interface_, dispatcher_, metrics_, manager_,
        manager_->device_info(), manager_->glib()));
  } else {
    Error::PopulateAndLog(
        error, Error::kNotSupported, "Unsupported VPN type: " + type);
    return NULL;
  }

  VPNServiceRefPtr service = new VPNService(
      control_interface_, dispatcher_, metrics_, manager_, driver.release());
  service->set_storage_id(storage_id);
  service->InitDriverPropertyStore();
  if (!name.empty()) {
    service->set_friendly_name(name);
  }
  services_.push_back(service);
  manager_->RegisterService(service);

  return service;
}

VPNServiceRefPtr VPNProvider::FindService(const std::string &type,
                                          const std::string &storage_id) {
  for (vector<VPNServiceRefPtr>::const_iterator it = services_.begin();
       it != services_.end();
       ++it) {
    if (type == (*it)->driver()->GetProviderType() &&
        storage_id == (*it)->GetStorageIdentifier()) {
      return *it;
    }
  }

  return NULL;
}

}  // namespace shill

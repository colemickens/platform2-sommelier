// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_service.h"

#include <algorithm>

#include <base/logging.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/technology.h"
#include "shill/vpn_driver.h"

using base::StringPrintf;
using std::replace_if;
using std::string;

namespace shill {

VPNService::VPNService(ControlInterface *control,
                       EventDispatcher *dispatcher,
                       Metrics *metrics,
                       Manager *manager,
                       VPNDriver *driver)
    : Service(control, dispatcher, metrics, manager, Technology::kVPN),
      driver_(driver) {
  mutable_store()->RegisterString(flimflam::kVPNDomainProperty, &vpn_domain_);
}

VPNService::~VPNService() {}

void VPNService::Connect(Error *error) {
  Service::Connect(error);
  driver_->Connect(this, error);
}

void VPNService::Disconnect(Error *error) {
  Service::Disconnect(error);
  driver_->Disconnect();
}

string VPNService::GetStorageIdentifier() const {
  return storage_id_;
}

// static
string VPNService::CreateStorageIdentifier(const KeyValueStore &args,
                                           Error *error) {
  string host = args.LookupString(flimflam::kProviderHostProperty, "");
  if (host.empty()) {
    Error::PopulateAndLog(
        error, Error::kInvalidProperty, "Missing VPN host.");
    return "";
  }
  string name = args.LookupString(flimflam::kProviderNameProperty, "");
  if (name.empty()) {
    name = args.LookupString(flimflam::kNameProperty, "");
    if (name.empty()) {
      Error::PopulateAndLog(
          error, Error::kNotSupported, "Missing VPN name.");
      return "";
    }
  }
  string id = StringPrintf("vpn_%s_%s", host.c_str(), name.c_str());
  replace_if(id.begin(), id.end(), &Service::IllegalChar, '_');
  return id;
}

string VPNService::GetDeviceRpcId(Error *error) {
  NOTIMPLEMENTED();
  error->Populate(Error::kNotSupported);
  return "/";
}

bool VPNService::Load(StoreInterface *storage) {
  return Service::Load(storage) &&
      driver_->Load(storage, GetStorageIdentifier());
}

bool VPNService::Save(StoreInterface *storage) {
  return Service::Save(storage) &&
      driver_->Save(storage, GetStorageIdentifier());
}

bool VPNService::Unload() {
  Service::Unload();

  // VPN services which have been removed from the profile should be
  // disconnected.
  driver_->Disconnect();

  // Ask the VPN provider to remove us from its list.
  manager()->vpn_provider()->RemoveService(this);

  return true;
}

void VPNService::InitDriverPropertyStore() {
  driver_->InitPropertyStore(mutable_store());
}

}  // namespace shill

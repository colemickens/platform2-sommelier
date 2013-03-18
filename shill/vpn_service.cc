// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn_service.h"

#include <algorithm>

#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/technology.h"
#include "shill/vpn_driver.h"
#include "shill/vpn_provider.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::replace_if;
using std::string;

namespace shill {

const char VPNService::kAutoConnNeverConnected[] = "never connected";
const char VPNService::kAutoConnVPNAlreadyActive[] = "vpn already active";

VPNService::VPNService(ControlInterface *control,
                       EventDispatcher *dispatcher,
                       Metrics *metrics,
                       Manager *manager,
                       VPNDriver *driver)
    : Service(control, dispatcher, metrics, manager, Technology::kVPN),
      driver_(driver) {
  set_connectable(true);
  set_save_credentials(false);
  mutable_store()->RegisterString(flimflam::kVPNDomainProperty, &vpn_domain_);
}

VPNService::~VPNService() {}

void VPNService::Connect(Error *error) {
  LOG(INFO) << "Connect to service " << unique_name();
  if (IsConnected() || IsConnecting()) {
    Error::PopulateAndLog(
        error, Error::kAlreadyConnected, "VPN service already connected.");
    return;
  }
  Service::Connect(error);
  driver_->Connect(this, error);
}

void VPNService::Disconnect(Error *error) {
  LOG(INFO) << "Disconnect from service " << unique_name();
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
  error->Populate(Error::kNotSupported);
  return "/";
}

bool VPNService::Load(StoreInterface *storage) {
  return Service::Load(storage) &&
      driver_->Load(storage, GetStorageIdentifier());
}

bool VPNService::Save(StoreInterface *storage) {
  return Service::Save(storage) &&
      driver_->Save(storage, GetStorageIdentifier(), save_credentials());
}

bool VPNService::Unload() {
  // The base method also disconnects the service.
  Service::Unload();

  set_save_credentials(false);
  driver_->UnloadCredentials();

  // Ask the VPN provider to remove us from its list.
  manager()->vpn_provider()->RemoveService(this);

  return true;
}

void VPNService::InitDriverPropertyStore() {
  driver_->InitPropertyStore(mutable_store());
}

void VPNService::MakeFavorite() {
  // The base MakeFavorite method also sets auto_connect_ to true
  // which is not desirable for VPN services.
  set_favorite(true);
}

void VPNService::SetConnection(const ConnectionRefPtr &connection) {
  // Construct the connection binder here rather than in the constructor because
  // there's really no reason to construct a binder if we never connect to this
  // service. It's safe to use an unretained callback to driver's method because
  // both the binder and the driver will be destroyed when this service is
  // destructed.
  if (!connection_binder_.get()) {
    connection_binder_.reset(
        new Connection::Binder(unique_name(),
                               Bind(&VPNDriver::OnConnectionDisconnected,
                                    Unretained(driver_.get()))));
  }
  // Note that |connection_| is a reference-counted pointer and is always set
  // through this method. This means that the connection binder will not be
  // notified when the connection is destructed (because we will unbind it first
  // here when it's set to NULL, or because the binder will already be destroyed
  // by ~VPNService) -- it will be notified only if the connection disconnects
  // (e.g., because an underlying connection is destructed).
  connection_binder_->Attach(connection);
  Service::SetConnection(connection);
}

bool VPNService::IsAutoConnectable(const char **reason) const {
  if (!Service::IsAutoConnectable(reason)) {
    return false;
  }
  // Don't auto-connect VPN services that have never connected. This improves
  // the chances that the VPN service is connectable and avoids dialog popups.
  if (!has_ever_connected()) {
    *reason = kAutoConnNeverConnected;
    return false;
  }
  // Don't auto-connect a VPN service if another VPN service is already active.
  if (manager()->vpn_provider()->HasActiveService()) {
    *reason = kAutoConnVPNAlreadyActive;
    return false;
  }
  return true;
}

}  // namespace shill

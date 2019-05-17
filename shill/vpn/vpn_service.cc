// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/vpn/vpn_service.h"

#include <algorithm>

#include <base/strings/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/profile.h"
#include "shill/property_accessor.h"
#include "shill/technology.h"
#include "shill/vpn/vpn_driver.h"
#include "shill/vpn/vpn_provider.h"

using base::Bind;
using base::StringPrintf;
using base::Unretained;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kVPN;
static string ObjectID(const VPNService* s) { return s->GetRpcIdentifier(); }
}

const char VPNService::kAutoConnNeverConnected[] = "never connected";
const char VPNService::kAutoConnVPNAlreadyActive[] = "vpn already active";

VPNService::VPNService(Manager* manager, VPNDriver* driver)
    : Service(manager, Technology::kVPN), driver_(driver) {
  SetConnectable(true);
  set_save_credentials(false);
  mutable_store()->RegisterDerivedString(
          kPhysicalTechnologyProperty,
          StringAccessor(
              new CustomAccessor<VPNService, string>(
                  this,
                  &VPNService::GetPhysicalTechnologyProperty,
                  nullptr)));
}

VPNService::~VPNService() = default;

void VPNService::Connect(Error* error, const char* reason) {
  if (IsConnected()) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kAlreadyConnected,
                          StringPrintf("VPN service %s already connected.",
                                       unique_name().c_str()));
    return;
  }
  if (IsConnecting()) {
    Error::PopulateAndLog(FROM_HERE, error, Error::kInProgress,
                          StringPrintf("VPN service %s already connecting.",
                                       unique_name().c_str()));
    return;
  }
  manager()->vpn_provider()->DisconnectAll();
  Service::Connect(error, reason);
  driver_->Connect(this, error);
}

void VPNService::Disconnect(Error* error, const char* reason) {
  SLOG(this, 1) << "Disconnect from service " << unique_name();
  Service::Disconnect(error, reason);
  driver_->Disconnect();
}

string VPNService::GetStorageIdentifier() const {
  return storage_id_;
}

bool VPNService::IsAlwaysOnVpn(const string& package) const {
  // For ArcVPN connections, the driver host is set to the package name of the
  // Android app that is creating the VPN connection.
  return driver_->GetProviderType() == string(kProviderArcVpn) &&
      driver_->GetHost() == package;
}

// static
string VPNService::CreateStorageIdentifier(const KeyValueStore& args,
                                           Error* error) {
  string host = args.LookupString(kProviderHostProperty, "");
  if (host.empty()) {
    Error::PopulateAndLog(
        FROM_HERE, error, Error::kInvalidProperty, "Missing VPN host.");
    return "";
  }
  string name = args.LookupString(kNameProperty, "");
  if (name.empty()) {
    Error::PopulateAndLog(
        FROM_HERE, error, Error::kNotSupported, "Missing VPN name.");
    return "";
  }
  return SanitizeStorageIdentifier(
      StringPrintf("vpn_%s_%s", host.c_str(), name.c_str()));
}

string VPNService::GetPhysicalTechnologyProperty(Error* error) {
  ConnectionConstRefPtr underlying_connection = GetUnderlyingConnection();
  if (!underlying_connection) {
    error->Populate(Error::kOperationFailed);
    return "";
  }

  return Technology::NameFromIdentifier(underlying_connection->technology());
}

RpcIdentifier VPNService::GetDeviceRpcId(Error* error) const {
  error->Populate(Error::kNotSupported);
  return RpcIdentifier("/");
}

ConnectionConstRefPtr VPNService::GetUnderlyingConnection() const {
  // TODO(crbug.com/941597) Policy routing should be used to enforce that VPN
  // traffic can only exit the interface it is supposed to. The VPN driver
  // should also be informed of changes in underlying connection.
  ServiceRefPtr underlying_service = manager()->GetPrimaryPhysicalService();
  if (!underlying_service) {
    return nullptr;
  }
  return underlying_service->connection();
}

bool VPNService::Load(StoreInterface* storage) {
  return Service::Load(storage) &&
      driver_->Load(storage, GetStorageIdentifier());
}

bool VPNService::Save(StoreInterface* storage) {
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

void VPNService::EnableAndRetainAutoConnect() {
  // The base EnableAndRetainAutoConnect method also sets auto_connect_ to true
  // which is not desirable for VPN services.
  RetainAutoConnect();
}

void VPNService::SetConnection(const ConnectionRefPtr& connection) {
  // Construct the connection binder here rather than in the constructor because
  // there's really no reason to construct a binder if we never connect to this
  // service. It's safe to use an unretained callback to driver's method because
  // both the binder and the driver will be destroyed when this service is
  // destructed.
  if (!connection_binder_) {
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

bool VPNService::IsAutoConnectable(const char** reason) const {
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

string VPNService::GetTethering(Error* error) const {
  ConnectionConstRefPtr underlying_connection = GetUnderlyingConnection();
  string tethering;
  if (underlying_connection) {
    tethering = underlying_connection->tethering();
    if (!tethering.empty()) {
      return tethering;
    }
    // The underlying service may not have a Tethering property.  This is
    // not strictly an error, so we don't print an error message.  Populating
    // an error here just serves to propagate the lack of a property in
    // GetProperties().
    error->Populate(Error::kNotSupported);
  } else {
    error->Populate(Error::kOperationFailed);
  }
  return "";
}

bool VPNService::SetNameProperty(const string& name, Error* error) {
  if (name == friendly_name()) {
    return false;
  }
  LOG(INFO) << "Renaming service " << unique_name() << ": "
            << friendly_name() << " -> " << name;

  KeyValueStore* args = driver_->args();
  args->SetString(kNameProperty, name);
  string new_storage_id = CreateStorageIdentifier(*args, error);
  if (new_storage_id.empty()) {
    return false;
  }
  string old_storage_id = storage_id_;
  DCHECK_NE(old_storage_id, new_storage_id);

  SetFriendlyName(name);

  // Update the storage identifier before invoking DeleteEntry to prevent it
  // from unloading this service.
  storage_id_ = new_storage_id;
  profile()->DeleteEntry(old_storage_id, nullptr);
  profile()->UpdateService(this);
  return true;
}

void VPNService::OnBeforeSuspend(const ResultCallback& callback) {
  driver_->OnBeforeSuspend(callback);
}

void VPNService::OnAfterResume() {
  driver_->OnAfterResume();
  Service::OnAfterResume();
}

void VPNService::OnDefaultServiceStateChanged(const ServiceRefPtr& service) {
  driver_->OnDefaultServiceStateChanged(service);
}

}  // namespace shill

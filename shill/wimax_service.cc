// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax_service.h"

#include <algorithm>

#include <base/string_number_conversions.h>
#include <base/string_util.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/service_constants.h>

#include "shill/key_value_store.h"
#include "shill/manager.h"
#include "shill/scope_logger.h"
#include "shill/store_interface.h"
#include "shill/technology.h"
#include "shill/wimax.h"

using std::replace_if;
using std::string;

namespace shill {

const char WiMaxService::kStorageNetworkId[] = "NetworkId";
const char WiMaxService::kNetworkIdProperty[] = "NetworkId";

WiMaxService::WiMaxService(ControlInterface *control,
                           EventDispatcher *dispatcher,
                           Metrics *metrics,
                           Manager *manager)
    : Service(control, dispatcher, metrics, manager, Technology::kWiMax),
      need_passphrase_(true),
      is_default_(false) {
  PropertyStore *store = this->mutable_store();
  // TODO(benchan): Support networks that require no user credentials or
  // implicitly defined credentials.
  store->RegisterBool(flimflam::kPassphraseRequiredProperty, &need_passphrase_);
  store->RegisterConstString(kNetworkIdProperty, &network_id_);

  IgnoreParameterForConfigure(kNetworkIdProperty);

  // Initialize a default storage identifier based on the service's unique
  // name. The identifier most likely needs to be reinitialized by the caller
  // when its components have been set.
  InitStorageIdentifier();
}

WiMaxService::~WiMaxService() {}

void WiMaxService::GetConnectParameters(KeyValueStore *parameters) const {
  CHECK(parameters);
  if (!eap().anonymous_identity.empty()) {
    parameters->SetString(wimax_manager::kEAPAnonymousIdentity,
                          eap().anonymous_identity);
  }
  if (!eap().identity.empty()) {
    parameters->SetString(wimax_manager::kEAPUserIdentity, eap().identity);
  }
  if (!eap().password.empty()) {
    parameters->SetString(wimax_manager::kEAPUserPassword, eap().password);
  }
}

RpcIdentifier WiMaxService::GetNetworkObjectPath() const {
  CHECK(proxy_.get());
  return proxy_->path();
}

void WiMaxService::Stop() {
  if (!IsStarted()) {
    return;
  }
  LOG(INFO) << "Stopping WiMAX service: " << GetStorageIdentifier();
  proxy_.reset();
  SetStrength(0);
  if (device_) {
    device_->OnServiceStopped(this);
    device_ = NULL;
  }
  UpdateConnectable();
}

bool WiMaxService::Start(WiMaxNetworkProxyInterface *proxy) {
  SLOG(WiMax, 2) << __func__;
  CHECK(proxy);
  scoped_ptr<WiMaxNetworkProxyInterface> local_proxy(proxy);
  if (IsStarted()) {
    return true;
  }
  if (friendly_name().empty()) {
    LOG(ERROR) << "Empty service name.";
    return false;
  }
  Error error;
  network_name_ = proxy->Name(&error);
  if (error.IsFailure()) {
    return false;
  }
  uint32 identifier = proxy->Identifier(&error);
  if (error.IsFailure()) {
    return false;
  }
  WiMaxNetworkId id = ConvertIdentifierToNetworkId(identifier);
  if (id != network_id_) {
    LOG(ERROR) << "Network identifiers don't match: "
               << id << " != " << network_id_;
    return false;
  }
  int signal_strength = proxy->SignalStrength(&error);
  if (error.IsFailure()) {
    return false;
  }
  SetStrength(signal_strength);
  proxy->set_signal_strength_changed_callback(
      Bind(&WiMaxService::OnSignalStrengthChanged, Unretained(this)));
  UpdateConnectable();
  proxy_.reset(local_proxy.release());
  LOG(INFO) << "WiMAX service started: " << GetStorageIdentifier();
  return true;
}

bool WiMaxService::IsStarted() const {
  return proxy_.get();
}

bool WiMaxService::TechnologyIs(const Technology::Identifier type) const {
  return type == Technology::kWiMax;
}

void WiMaxService::Connect(Error *error) {
  if (device_) {
    Error::PopulateAndLog(
        error, Error::kAlreadyConnected, "Already connected.");
    return;
  }
  device_ = manager()->wimax_provider()->SelectCarrier(this);
  if (!device_) {
    Error::PopulateAndLog(
        error, Error::kNoCarrier, "No suitable WiMAX device available.");
    return;
  }
  Service::Connect(error);
  device_->ConnectTo(this, error);
}

void WiMaxService::Disconnect(Error *error) {
  if (!device_) {
    Error::PopulateAndLog(
        error, Error::kNotConnected, "Not connected.");
    return;
  }
  Service::Disconnect(error);
  device_->DisconnectFrom(this, error);
  device_ = NULL;
}

string WiMaxService::GetStorageIdentifier() const {
  return storage_id_;
}

string WiMaxService::GetDeviceRpcId(Error *error) {
  if (device_) {
    return device_->GetRpcIdentifier();
  }
  error->Populate(Error::kNotSupported);
  return "/";
}

bool WiMaxService::Is8021x() const {
  return true;
}

void WiMaxService::set_eap(const EapCredentials &eap) {
  Service::set_eap(eap);
  UpdateConnectable();
}

void WiMaxService::UpdateConnectable() {
  // Don't use Service::Is8021xConnectable because we don't support the full set
  // of authentication methods.
  bool is_connectable = false;
  if (IsStarted()) {
    if (!eap().identity.empty()) {
      is_connectable = !eap().password.empty();
    }
  }
  set_connectable(is_connectable);
}

void WiMaxService::OnSignalStrengthChanged(int strength) {
  SLOG(WiMax, 2) << __func__ << "(" << strength << ")";
  SetStrength(strength);
}

bool WiMaxService::Save(StoreInterface *storage) {
  SLOG(WiMax, 2) << __func__;
  if (!Service::Save(storage)) {
    return false;
  }
  const string id = GetStorageIdentifier();
  storage->SetString(id, kStorageNetworkId, network_id_);
  return true;
}

bool WiMaxService::Unload() {
  // The base method also disconnects the service.
  Service::Unload();
  // Notify the WiMAX provider that this service has been unloaded. If the
  // provider releases ownership of this service, it needs to be deregistered.
  return manager()->wimax_provider()->OnServiceUnloaded(this);
}

// static
WiMaxNetworkId WiMaxService::ConvertIdentifierToNetworkId(uint32 identifier) {
  return base::StringPrintf("%08x", identifier);
}

void WiMaxService::InitStorageIdentifier() {
  storage_id_ = CreateStorageIdentifier(network_id_, friendly_name());
}

// static
string WiMaxService::CreateStorageIdentifier(const WiMaxNetworkId &id,
                                             const string &name) {
  string storage_id =
      base::StringPrintf("%s_%s_%s",
                         flimflam::kTypeWimax, name.c_str(), id.c_str());
  StringToLowerASCII(&storage_id);
  replace_if(storage_id.begin(), storage_id.end(), &Service::IllegalChar, '_');
  return storage_id;
}

}  // namespace shill

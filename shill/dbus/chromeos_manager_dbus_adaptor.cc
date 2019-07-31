// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_manager_dbus_adaptor.h"

#include <string>
#include <utility>
#include <vector>

#include "shill/callbacks.h"
#include "shill/dbus/dbus_service_watcher_factory.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/geolocation_info.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/property_store.h"

using base::Unretained;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosManagerDBusAdaptor* m) {
  return m->GetRpcIdentifier();
}
}  // namespace Logging

// static
const char ChromeosManagerDBusAdaptor::kPath[] = "/";

ChromeosManagerDBusAdaptor::ChromeosManagerDBusAdaptor(
    const scoped_refptr<dbus::Bus>& adaptor_bus,
    const scoped_refptr<dbus::Bus> proxy_bus,
    Manager* manager)
    : org::chromium::flimflam::ManagerAdaptor(this),
      ChromeosDBusAdaptor(adaptor_bus, kPath),
      manager_(manager),
      proxy_bus_(proxy_bus),
      dbus_service_watcher_factory_(DBusServiceWatcherFactory::GetInstance()) {}

ChromeosManagerDBusAdaptor::~ChromeosManagerDBusAdaptor() {
  manager_ = nullptr;
}

void ChromeosManagerDBusAdaptor::RegisterAsync(
    const base::Callback<void(bool)>& completion_callback) {
  RegisterWithDBusObject(dbus_object());
  dbus_object()->RegisterAsync(completion_callback);
}

void ChromeosManagerDBusAdaptor::EmitBoolChanged(const string& name,
                                                 bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitUintChanged(const string& name,
                                                 uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitStringChanged(const string& name,
                                                   const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitStringsChanged(
    const string& name, const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitRpcIdentifierChanged(
    const string& name, const RpcIdentifier& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(dbus::ObjectPath(value)));
}

void ChromeosManagerDBusAdaptor::EmitRpcIdentifierArrayChanged(
    const string& name, const RpcIdentifiers& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  vector<dbus::ObjectPath> paths;
  for (const auto& element : value) {
    paths.push_back(dbus::ObjectPath(element));
  }

  SendPropertyChangedSignal(name, brillo::Any(paths));
}

bool ChromeosManagerDBusAdaptor::GetProperties(
    brillo::ErrorPtr* error, brillo::VariantDictionary* properties) {
  SLOG(this, 2) << __func__;
  return ChromeosDBusAdaptor::GetProperties(manager_->store(), properties,
                                            error);
}

bool ChromeosManagerDBusAdaptor::SetProperty(brillo::ErrorPtr* error,
                                             const string& name,
                                             const brillo::Any& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::SetProperty(manager_->mutable_store(), name,
                                          value, error);
}

bool ChromeosManagerDBusAdaptor::GetState(brillo::ErrorPtr* /*error*/,
                                          string* state) {
  SLOG(this, 2) << __func__;
  *state = manager_->CalculateState(nullptr);
  return true;
}

bool ChromeosManagerDBusAdaptor::CreateProfile(brillo::ErrorPtr* error,
                                               const string& name,
                                               dbus::ObjectPath* profile_path) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  string path;
  manager_->CreateProfile(name, &path, &e);
  if (e.ToChromeosError(error)) {
    return false;
  }
  *profile_path = dbus::ObjectPath(path);
  return true;
}

bool ChromeosManagerDBusAdaptor::RemoveProfile(brillo::ErrorPtr* error,
                                               const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  manager_->RemoveProfile(name, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::PushProfile(brillo::ErrorPtr* error,
                                             const string& name,
                                             dbus::ObjectPath* profile_path) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  string path;
  manager_->PushProfile(name, &path, &e);
  if (e.ToChromeosError(error)) {
    return false;
  }
  *profile_path = dbus::ObjectPath(path);
  return true;
}

bool ChromeosManagerDBusAdaptor::InsertUserProfile(
    brillo::ErrorPtr* error,
    const string& name,
    const string& user_hash,
    dbus::ObjectPath* profile_path) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  string path;
  manager_->InsertUserProfile(name, user_hash, &path, &e);
  if (e.ToChromeosError(error)) {
    return false;
  }
  *profile_path = dbus::ObjectPath(path);
  return true;
}

bool ChromeosManagerDBusAdaptor::PopProfile(brillo::ErrorPtr* error,
                                            const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  manager_->PopProfile(name, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::PopAnyProfile(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->PopAnyProfile(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::PopAllUserProfiles(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->PopAllUserProfiles(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::RecheckPortal(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->RecheckPortal(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::RequestScan(
    brillo::ErrorPtr* error,
    const string& technology) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << technology;
  Error e;
  manager_->RequestScan(technology, &e);
  return !e.ToChromeosError(error);
}

void ChromeosManagerDBusAdaptor::SetNetworkThrottlingStatus(
    DBusMethodResponsePtr<> response,
    bool enabled,
    uint32_t upload_rate_kbits,
    uint32_t download_rate_kbits) {
  SLOG(this, 2) << __func__ << ": " << enabled;
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  manager_->SetNetworkThrottlingStatus(callback, enabled, upload_rate_kbits,
                                       download_rate_kbits);
  return;
}

void ChromeosManagerDBusAdaptor::EnableTechnology(
    DBusMethodResponsePtr<> response, const string& technology_name) {
  SLOG(this, 2) << __func__ << ": " << technology_name;
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  const bool kPersistentSave = true;
  manager_->SetEnabledStateForTechnology(technology_name, true, kPersistentSave,
                                         &e, callback);
  ReturnResultOrDefer(callback, e);
}

void ChromeosManagerDBusAdaptor::DisableTechnology(
    DBusMethodResponsePtr<> response, const string& technology_name) {
  SLOG(this, 2) << __func__ << ": " << technology_name;
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  const bool kPersistentSave = true;
  manager_->SetEnabledStateForTechnology(technology_name, false,
                                         kPersistentSave, &e, callback);
  ReturnResultOrDefer(callback, e);
}

// Called, e.g., to get WiFiService handle for a hidden SSID.
bool ChromeosManagerDBusAdaptor::GetService(
    brillo::ErrorPtr* error,
    const brillo::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  Error e;
  KeyValueStore args_store = KeyValueStore::ConvertFromVariantDictionary(args);
  service = manager_->GetService(args_store, &e);
  if (e.ToChromeosError(error)) {
    return false;
  }
  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::ConfigureService(
    brillo::ErrorPtr* error,
    const brillo::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store = KeyValueStore::ConvertFromVariantDictionary(args);
  Error configure_error;
  service = manager_->ConfigureService(args_store, &configure_error);
  if (configure_error.ToChromeosError(error)) {
    return false;
  }
  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::ConfigureServiceForProfile(
    brillo::ErrorPtr* error,
    const dbus::ObjectPath& profile_rpcid,
    const brillo::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store = KeyValueStore::ConvertFromVariantDictionary(args);
  Error configure_error;
  service = manager_->ConfigureServiceForProfile(profile_rpcid.value(),
                                                 args_store, &configure_error);
  if (configure_error.ToChromeosError(error)) {
    return false;
  }
  CHECK(service);
  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::FindMatchingService(
    brillo::ErrorPtr* error,
    const brillo::VariantDictionary& args,
    dbus::ObjectPath* service_path) {  // NOLINT
  SLOG(this, 2) << __func__;
  KeyValueStore args_store = KeyValueStore::ConvertFromVariantDictionary(args);

  Error find_error;
  ServiceRefPtr service =
      manager_->FindMatchingService(args_store, &find_error);
  if (find_error.ToChromeosError(error)) {
    return false;
  }

  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::GetDebugLevel(brillo::ErrorPtr* /*error*/,
                                               int32_t* level) {
  SLOG(this, 2) << __func__;
  *level = logging::GetMinLogLevel();
  return true;
}

bool ChromeosManagerDBusAdaptor::SetDebugLevel(brillo::ErrorPtr* /*error*/,
                                               int32_t level) {
  SLOG(this, 2) << __func__ << ": " << level;
  if (level < logging::LOG_NUM_SEVERITIES) {
    logging::SetMinLogLevel(level);
    // Like VLOG, SLOG uses negative verbose level.
    ScopeLogger::GetInstance()->set_verbose_level(-level);
  } else {
    LOG(WARNING) << "Ignoring attempt to set log level to " << level;
  }
  return true;
}

bool ChromeosManagerDBusAdaptor::GetServiceOrder(brillo::ErrorPtr* /*error*/,
                                                 string* order) {
  SLOG(this, 2) << __func__;
  *order = manager_->GetTechnologyOrder();
  return true;
}

bool ChromeosManagerDBusAdaptor::SetServiceOrder(brillo::ErrorPtr* error,
                                                 const string& order) {
  SLOG(this, 2) << __func__ << ": " << order;
  Error e;
  manager_->SetTechnologyOrder(order, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::GetDebugTags(brillo::ErrorPtr* /*error*/,
                                              string* tags) {
  SLOG(this, 2) << __func__;
  *tags = ScopeLogger::GetInstance()->GetEnabledScopeNames();
  return true;
}

bool ChromeosManagerDBusAdaptor::SetDebugTags(brillo::ErrorPtr* /*error*/,
                                              const string& tags) {
  SLOG(this, 2) << __func__ << ": " << tags;
  ScopeLogger::GetInstance()->EnableScopesByName(tags);
  return true;
}

bool ChromeosManagerDBusAdaptor::ListDebugTags(brillo::ErrorPtr* /*error*/,
                                               string* tags) {
  SLOG(this, 2) << __func__;
  *tags = ScopeLogger::GetInstance()->GetAllScopeNames();
  return true;
}

bool ChromeosManagerDBusAdaptor::GetNetworksForGeolocation(
    brillo::ErrorPtr* /*error*/, brillo::VariantDictionary* networks) {
  SLOG(this, 2) << __func__;
  for (const auto& network : manager_->GetNetworksForGeolocation()) {
    networks->emplace(network.first, brillo::Any(network.second));
  }
  return true;
}

bool ChromeosManagerDBusAdaptor::ConnectToBestServices(
    brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->ConnectToBestServices(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::CreateConnectivityReport(
    brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->CreateConnectivityReport(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::ClaimInterface(brillo::ErrorPtr* error,
                                                dbus::Message* message,
                                                const string& claimer_name,
                                                const string& interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  // Empty claimer name is used to indicate default claimer.
  // TODO(samueltan): update this API or make a new API to use a flag to
  // indicate default claimer instead (b/27924738).
  string claimer = (claimer_name == "" ? "" : message->GetSender());
  manager_->ClaimDevice(claimer, interface_name, &e);
  if (e.IsSuccess() && claimer_name != "") {
    // Only setup watcher for non-default claimer.
    watcher_for_device_claimer_ =
        dbus_service_watcher_factory_->CreateDBusServiceWatcher(
            proxy_bus_, claimer,
            Bind(&ChromeosManagerDBusAdaptor::OnDeviceClaimerVanished,
                 Unretained(this)));
  }
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::ReleaseInterface(
    brillo::ErrorPtr* error,
    dbus::Message* message,
    const string& claimer_name,
    const string& interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  bool claimer_removed;
  // Empty claimer name is used to indicate default claimer.
  // TODO(samueltan): update this API or make a new API to use a flag to
  // indicate default claimer instead (b/27924738).
  manager_->ReleaseDevice(claimer_name == "" ? "" : message->GetSender(),
                          interface_name, &claimer_removed, &e);
  if (claimer_removed) {
    watcher_for_device_claimer_.reset();
  }
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::SetSchedScan(brillo::ErrorPtr* error,
                                              bool enable) {
  SLOG(this, 2) << __func__ << ": " << enable;
  Error e;
  manager_->SetSchedScan(enable, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::SetupApModeInterface(
    brillo::ErrorPtr* error,
    dbus::Message* message,
    string* out_interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  e.Populate(Error::kNotSupported);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::SetupStationModeInterface(
    brillo::ErrorPtr* error, string* out_interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  e.Populate(Error::kNotSupported);
  return !e.ToChromeosError(error);
}

void ChromeosManagerDBusAdaptor::OnApModeSetterVanished() {
  SLOG(this, 3) << __func__;
  watcher_for_ap_mode_setter_.reset();
}

void ChromeosManagerDBusAdaptor::OnDeviceClaimerVanished() {
  SLOG(this, 3) << __func__;
  manager_->OnDeviceClaimerVanished();
  watcher_for_device_claimer_.reset();
}

}  // namespace shill

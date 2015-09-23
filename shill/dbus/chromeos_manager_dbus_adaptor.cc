//
// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "shill/dbus/chromeos_manager_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include "shill/callbacks.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/geolocation_info.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"
#include "shill/property_store.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosManagerDBusAdaptor* m) {
  return m->GetRpcIdentifier();
}
}

// static
const char ChromeosManagerDBusAdaptor::kPath[] = "/";

ChromeosManagerDBusAdaptor::ChromeosManagerDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    Manager* manager)
    : org::chromium::flimflam::ManagerAdaptor(this),
      ChromeosDBusAdaptor(bus, kPath),
      manager_(manager) {
}

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
  SendPropertyChangedSignal(name, chromeos::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitUintChanged(const string& name,
                                         uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, chromeos::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, chromeos::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitStringChanged(const string& name,
                                           const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, chromeos::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitStringsChanged(const string& name,
                                            const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, chromeos::Any(value));
}

void ChromeosManagerDBusAdaptor::EmitRpcIdentifierChanged(
    const string& name,
    const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, chromeos::Any(dbus::ObjectPath(value)));
}

void ChromeosManagerDBusAdaptor::EmitRpcIdentifierArrayChanged(
    const string& name,
    const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  vector<dbus::ObjectPath> paths;
  for (const auto& element : value) {
    paths.push_back(dbus::ObjectPath(element));
  }

  SendPropertyChangedSignal(name, chromeos::Any(paths));
}

void ChromeosManagerDBusAdaptor::EmitStateChanged(const string& new_state) {
  SLOG(this, 2) << __func__;
  SendStateChangedSignal(new_state);
}

bool ChromeosManagerDBusAdaptor::GetProperties(
    chromeos::ErrorPtr* error, chromeos::VariantDictionary* properties) {
  SLOG(this, 2) << __func__;
  return ChromeosDBusAdaptor::GetProperties(manager_->store(),
                                            properties,
                                            error);
}

bool ChromeosManagerDBusAdaptor::SetProperty(chromeos::ErrorPtr* error,
                                             const string& name,
                                             const chromeos::Any& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::SetProperty(manager_->mutable_store(),
                                          name,
                                          value,
                                          error);
}

bool ChromeosManagerDBusAdaptor::GetState(chromeos::ErrorPtr* /*error*/,
                                          string* state) {
  SLOG(this, 2) << __func__;
  *state = manager_->CalculateState(nullptr);
  return true;
}

bool ChromeosManagerDBusAdaptor::CreateProfile(chromeos::ErrorPtr* error,
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

bool ChromeosManagerDBusAdaptor::RemoveProfile(chromeos::ErrorPtr* error,
                                               const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  manager_->RemoveProfile(name, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::PushProfile(chromeos::ErrorPtr* error,
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
    chromeos::ErrorPtr* error,
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
  return true;;
}

bool ChromeosManagerDBusAdaptor::PopProfile(chromeos::ErrorPtr* error,
                                            const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  manager_->PopProfile(name, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::PopAnyProfile(chromeos::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->PopAnyProfile(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::PopAllUserProfiles(chromeos::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->PopAllUserProfiles(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::RecheckPortal(chromeos::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->RecheckPortal(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::RequestScan(chromeos::ErrorPtr* error,
                                             const string& technology) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << technology;
  Error e;
  manager_->RequestScan(Device::kFullScan, technology, &e);
  return !e.ToChromeosError(error);
}

void ChromeosManagerDBusAdaptor::EnableTechnology(
    DBusMethodResponsePtr<> response, const string& technology_name) {
  SLOG(this, 2) << __func__ << ": " << technology_name;
  Error e(Error::kOperationInitiated);
  ResultCallback callback = GetMethodReplyCallback(std::move(response));
  const bool kPersistentSave = true;
  manager_->SetEnabledStateForTechnology(technology_name, true,
                                         kPersistentSave, &e, callback);
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
    chromeos::ErrorPtr* error,
    const chromeos::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  Error e;
  KeyValueStore::ConvertFromVariantDictionary(args, &args_store);
  service = manager_->GetService(args_store, &e);
  if (e.ToChromeosError(error)) {
    return false;
  }
  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

// Obsolete, use GetService instead.
bool ChromeosManagerDBusAdaptor::GetVPNService(
    chromeos::ErrorPtr* error,
    const chromeos::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  return GetService(error, args, service_path);
}

// Obsolete, use GetService instead.
bool ChromeosManagerDBusAdaptor::GetWifiService(
    chromeos::ErrorPtr* error,
    const chromeos::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  return GetService(error, args, service_path);
}


bool ChromeosManagerDBusAdaptor::ConfigureService(
    chromeos::ErrorPtr* error,
    const chromeos::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  KeyValueStore::ConvertFromVariantDictionary(args, &args_store);
  Error configure_error;
  service = manager_->ConfigureService(args_store, &configure_error);
  if (configure_error.ToChromeosError(error)) {
    return false;
  }
  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::ConfigureServiceForProfile(
    chromeos::ErrorPtr* error,
    const dbus::ObjectPath& profile_rpcid,
    const chromeos::VariantDictionary& args,
    dbus::ObjectPath* service_path) {
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  KeyValueStore::ConvertFromVariantDictionary(args, &args_store);
  Error configure_error;
  service = manager_->ConfigureServiceForProfile(
      profile_rpcid.value(), args_store, &configure_error);
  if (!service || configure_error.ToChromeosError(error)) {
    return false;
  }
  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::FindMatchingService(
    chromeos::ErrorPtr* error,
    const chromeos::VariantDictionary& args,
    dbus::ObjectPath* service_path) {  // NOLINT
  SLOG(this, 2) << __func__;
  KeyValueStore args_store;
  KeyValueStore::ConvertFromVariantDictionary(args, &args_store);

  Error find_error;
  ServiceRefPtr service =
      manager_->FindMatchingService(args_store, &find_error);
  if (find_error.ToChromeosError(error)) {
    return false;
  }

  *service_path = dbus::ObjectPath(service->GetRpcIdentifier());
  return true;
}

bool ChromeosManagerDBusAdaptor::GetDebugLevel(chromeos::ErrorPtr* /*error*/,
                                       int32_t* level) {
  SLOG(this, 2) << __func__;
  *level = logging::GetMinLogLevel();
  return true;
}

bool ChromeosManagerDBusAdaptor::SetDebugLevel(chromeos::ErrorPtr* /*error*/,
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

bool ChromeosManagerDBusAdaptor::GetServiceOrder(chromeos::ErrorPtr* /*error*/,
                                                 string* order) {
  SLOG(this, 2) << __func__;
  *order = manager_->GetTechnologyOrder();
  return true;
}

bool ChromeosManagerDBusAdaptor::SetServiceOrder(chromeos::ErrorPtr* error,
                                                 const string& order) {
  SLOG(this, 2) << __func__ << ": " << order;
  Error e;
  manager_->SetTechnologyOrder(order, &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::GetDebugTags(chromeos::ErrorPtr* /*error*/,
                                              string* tags) {
  SLOG(this, 2) << __func__;
  *tags = ScopeLogger::GetInstance()->GetEnabledScopeNames();
  return true;
}

bool ChromeosManagerDBusAdaptor::SetDebugTags(chromeos::ErrorPtr* /*error*/,
                                              const string& tags) {
  SLOG(this, 2) << __func__ << ": " << tags;
  ScopeLogger::GetInstance()->EnableScopesByName(tags);
  return true;
}

bool ChromeosManagerDBusAdaptor::ListDebugTags(chromeos::ErrorPtr* /*error*/,
                                               string* tags) {
  SLOG(this, 2) << __func__;
  *tags = ScopeLogger::GetInstance()->GetAllScopeNames();
  return true;
}

bool ChromeosManagerDBusAdaptor::GetNetworksForGeolocation(
    chromeos::ErrorPtr* /*error*/,
    chromeos::VariantDictionary* networks) {
  SLOG(this, 2) << __func__;
  for (const auto& network : manager_->GetNetworksForGeolocation()) {
    Stringmaps value;
    // Convert GeolocationInfos to their Stringmaps equivalent.
    for (const auto& info : network.second) {
      value.push_back(info.properties());
    }
    networks->insert(std::make_pair(network.first, chromeos::Any(value)));
  }
  return true;
}

void ChromeosManagerDBusAdaptor::VerifyDestination(
    DBusMethodResponsePtr<bool> response,
    const string& certificate,
    const string& public_key,
    const string& nonce,
    const string& signed_data,
    const string& destination_udn,
    const string& hotspot_ssid,
    const string& hotspot_bssid) {
  SLOG(this, 2) << __func__;
  ResultBoolCallback callback = GetBoolMethodReplyCallback(std::move(response));
#if !defined(DISABLE_WIFI)
  Error e(Error::kOperationInitiated);
  manager_->VerifyDestination(certificate, public_key, nonce,
                              signed_data, destination_udn,
                              hotspot_ssid, hotspot_bssid,
                              callback, &e);
#else
  Error e(Error::kNotImplemented);
#endif  // DISABLE_WIFI
  if (e.IsOngoing()) {
    return;
  }
  // Command failed synchronously.
  CHECK(e.IsFailure()) << __func__ << " should only return directly on error.";
  callback.Run(e, false);
}

void ChromeosManagerDBusAdaptor::VerifyAndEncryptCredentials(
    DBusMethodResponsePtr<string> response,
    const string& certificate,
    const string& public_key,
    const string& nonce,
    const string& signed_data,
    const string& destination_udn,
    const string& hotspot_ssid,
    const string& hotspot_bssid,
    const dbus::ObjectPath& network) {
  SLOG(this, 2) << __func__;
  ResultStringCallback callback =
      GetStringMethodReplyCallback(std::move(response));
#if !defined(DISABLE_WIFI)
  Error e(Error::kOperationInitiated);
  manager_->VerifyAndEncryptCredentials(certificate, public_key, nonce,
                                        signed_data, destination_udn,
                                        hotspot_ssid, hotspot_bssid,
                                        network.value(),
                                        callback,
                                        &e);
#else
  Error e(Error::kNotImplemented);
#endif  // DISABLE_WIFI
  if (e.IsOngoing()) {
    return;
  }
  // Command failed synchronously.
  CHECK(e.IsFailure()) << __func__ << " should only return directly on error.";
  callback.Run(e, "");
}

void ChromeosManagerDBusAdaptor::VerifyAndEncryptData(
    DBusMethodResponsePtr<string> response,
    const string& certificate,
    const string& public_key,
    const string& nonce,
    const string& signed_data,
    const string& destination_udn,
    const string& hotspot_ssid,
    const string& hotspot_bssid,
    const string& data) {
  SLOG(this, 2) << __func__;
  ResultStringCallback callback =
      GetStringMethodReplyCallback(std::move(response));
#if !defined(DISABLE_WIFI)
  Error e(Error::kOperationInitiated);
  manager_->VerifyAndEncryptData(certificate, public_key, nonce,
                                 signed_data, destination_udn,
                                 hotspot_ssid, hotspot_bssid,
                                 data, callback,
                                 &e);
#else
  Error e(Error::kNotImplemented);
#endif  // DISABLE_WIFI
  if (e.IsOngoing()) {
    return;
  }
  // Command failed synchronously.
  CHECK(e.IsFailure()) << __func__ << " should only return directly on error.";
  callback.Run(e, "");
}

bool ChromeosManagerDBusAdaptor::ConnectToBestServices(
    chromeos::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->ConnectToBestServices(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::CreateConnectivityReport(
    chromeos::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  manager_->CreateConnectivityReport(&e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::ClaimInterface(
    chromeos::ErrorPtr* error,
    dbus::Message* message,
    const string& claimer_name,
    const string& interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  // Empty claimer name is used to indicate default claimer.
  // TODO(zqiu): update this API or make a new API to use a flag to indicate
  // default claimer instead.
  manager_->ClaimDevice(
      claimer_name == "" ? "" : message->GetSender(),
      interface_name,
      &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::ReleaseInterface(
    chromeos::ErrorPtr* error,
    dbus::Message* message,
    const string& claimer_name,
    const string& interface_name) {
  SLOG(this, 2) << __func__;
  Error e;
  // Empty claimer name is used to indicate default claimer.
  // TODO(zqiu): update this API or make a new API to use a flag to indicate
  // default claimer instead.
  manager_->ReleaseDevice(
      claimer_name == "" ? "" : message->GetSender(),
      interface_name,
      &e);
  return !e.ToChromeosError(error);
}

bool ChromeosManagerDBusAdaptor::SetSchedScan(chromeos::ErrorPtr* error,
                                              bool enable) {
  SLOG(this, 2) << __func__ << ": " << enable;
  Error e;
  manager_->SetSchedScan(enable, &e);
  return !e.ToChromeosError(error);
}

}  // namespace shill

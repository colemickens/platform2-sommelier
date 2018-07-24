// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/manager_dbus_adaptor.h"

#include <map>
#include <string>
#include <vector>

#include <dbus-c++/dbus.h>

#include "shill/callbacks.h"
#include "shill/device.h"
#include "shill/error.h"
#include "shill/geolocation_info.h"
#include "shill/key_value_store.h"
#include "shill/logging.h"
#include "shill/manager.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ManagerDBusAdaptor* m) { return m->GetRpcIdentifier(); }
}

// static
const char ManagerDBusAdaptor::kPath[] = "/";

ManagerDBusAdaptor::ManagerDBusAdaptor(DBus::Connection* conn, Manager* manager)
    : DBusAdaptor(conn, kPath),
      manager_(manager) {
}

ManagerDBusAdaptor::~ManagerDBusAdaptor() {
  manager_ = nullptr;
}

void ManagerDBusAdaptor::EmitBoolChanged(const string& name, bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void ManagerDBusAdaptor::EmitUintChanged(const string& name,
                                         uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void ManagerDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void ManagerDBusAdaptor::EmitStringChanged(const string& name,
                                           const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void ManagerDBusAdaptor::EmitStringsChanged(const string& name,
                                            const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringsToVariant(value));
}

void ManagerDBusAdaptor::EmitRpcIdentifierChanged(
    const string& name,
    const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::PathToVariant(value));
}

void ManagerDBusAdaptor::EmitRpcIdentifierArrayChanged(
    const string& name,
    const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  vector<DBus::Path> paths;
  for (const auto& element : value) {
    paths.push_back(element);
  }

  PropertyChanged(name, DBusAdaptor::PathsToVariant(paths));
}

void ManagerDBusAdaptor::EmitStateChanged(const string& new_state) {
  SLOG(this, 2) << __func__;
  StateChanged(new_state);
}

map<string, DBus::Variant> ManagerDBusAdaptor::GetProperties(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  map<string, DBus::Variant> properties;
  DBusAdaptor::GetProperties(manager_->store(), &properties, &error);
  return properties;
}

void ManagerDBusAdaptor::SetProperty(const string& name,
                                     const DBus::Variant& value,
                                     DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  if (DBusAdaptor::SetProperty(manager_->mutable_store(),
                               name,
                               value,
                               &error)) {
    PropertyChanged(name, value);
  }
}

string ManagerDBusAdaptor::GetState(DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
  return manager_->CalculateState(nullptr);
}

DBus::Path ManagerDBusAdaptor::CreateProfile(const string& name,
                                             DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  string path;
  manager_->CreateProfile(name, &path, &e);
  if (e.ToDBusError(&error)) {
    return "/";
  }
  return DBus::Path(path);
}

void ManagerDBusAdaptor::RemoveProfile(const string& name,
                                       DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  manager_->RemoveProfile(name, &e);
  e.ToDBusError(&error);
}

DBus::Path ManagerDBusAdaptor::PushProfile(const string& name,
                                           DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  string path;
  manager_->PushProfile(name, &path, &e);
  if (e.ToDBusError(&error)) {
    return "/";
  }
  return DBus::Path(path);
}

DBus::Path ManagerDBusAdaptor::InsertUserProfile(
    const string& name,
    const string& user_hash,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  string path;
  manager_->InsertUserProfile(name, user_hash, &path, &e);
  if (e.ToDBusError(&error)) {
    return "/";
  }
  return DBus::Path(path);
}

void ManagerDBusAdaptor::PopProfile(const string& name,
                                    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  manager_->PopProfile(name, &e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::PopAnyProfile(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  manager_->PopAnyProfile(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::PopAllUserProfiles(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  manager_->PopAllUserProfiles(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::RecheckPortal(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  manager_->RecheckPortal(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::RequestScan(const string& technology,
                                     DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << technology;
  Error e;
  manager_->RequestScan(Device::kFullScan, technology, &e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::EnableTechnology(const string& technology_name,
                                          DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << technology_name;
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  manager_->SetEnabledStateForTechnology(technology_name, true, &e,
                                         GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void ManagerDBusAdaptor::DisableTechnology(const string& technology_name,
                                           DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << technology_name;
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  manager_->SetEnabledStateForTechnology(technology_name, false, &e,
                                         GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

// Called, e.g., to get WiFiService handle for a hidden SSID.
DBus::Path ManagerDBusAdaptor::GetService(
    const map<string, DBus::Variant>& args,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  Error e;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_store, &e);
  if (e.IsSuccess()) {
    service = manager_->GetService(args_store, &e);
  }
  if (e.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid
  }
  return service->GetRpcIdentifier();
}

// Obsolete, use GetService instead.
DBus::Path ManagerDBusAdaptor::GetVPNService(
    const map<string, DBus::Variant>& args,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  return GetService(args, error);
}

// Obsolete, use GetService instead.
DBus::Path ManagerDBusAdaptor::GetWifiService(
    const map<string, DBus::Variant>& args,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  return GetService(args, error);
}


DBus::Path ManagerDBusAdaptor::ConfigureService(
    const map<string, DBus::Variant>& args,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  Error key_value_store_error;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_store, &key_value_store_error);
  if (key_value_store_error.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid.
  }
  Error configure_error;
  service = manager_->ConfigureService(args_store, &configure_error);
  if (configure_error.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid.
  }
  return service->GetRpcIdentifier();
}

DBus::Path ManagerDBusAdaptor::ConfigureServiceForProfile(
    const DBus::Path& profile_rpcid,
    const map<string, DBus::Variant>& args,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  ServiceRefPtr service;
  KeyValueStore args_store;
  Error key_value_store_error;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_store, &key_value_store_error);
  if (key_value_store_error.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid.
  }
  Error configure_error;
  service = manager_->ConfigureServiceForProfile(
      profile_rpcid, args_store, &configure_error);
  if (!service || configure_error.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid.
  }
  return service->GetRpcIdentifier();
}

DBus::Path ManagerDBusAdaptor::FindMatchingService(
    const map<string, DBus::Variant>& args,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  KeyValueStore args_store;
  Error value_error;
  DBusAdaptor::ArgsToKeyValueStore(args, &args_store, &value_error);
  if (value_error.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid
  }

  Error find_error;
  ServiceRefPtr service =
      manager_->FindMatchingService(args_store, &find_error);
  if (find_error.ToDBusError(&error)) {
    return "/";  // ensure return is syntactically valid
  }

  return service->GetRpcIdentifier();
}

int32_t ManagerDBusAdaptor::GetDebugLevel(DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
  return logging::GetMinLogLevel();
}

void ManagerDBusAdaptor::SetDebugLevel(const int32_t& level,
                                       DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << level;
  if (level < logging::LOG_NUM_SEVERITIES) {
    logging::SetMinLogLevel(level);
    // Like VLOG, SLOG uses negative verbose level.
    ScopeLogger::GetInstance()->set_verbose_level(-level);
  } else {
    LOG(WARNING) << "Ignoring attempt to set log level to " << level;
  }
}

string ManagerDBusAdaptor::GetServiceOrder(DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
  return manager_->GetTechnologyOrder();
}

void ManagerDBusAdaptor::SetServiceOrder(const string& order,
                                         DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << order;
  Error e;
  manager_->SetTechnologyOrder(order, &e);
  e.ToDBusError(&error);
}

string ManagerDBusAdaptor::GetDebugTags(DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
  return ScopeLogger::GetInstance()->GetEnabledScopeNames();
}

void ManagerDBusAdaptor::SetDebugTags(const string& tags,
                                      DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << tags;
  ScopeLogger::GetInstance()->EnableScopesByName(tags);
}

string ManagerDBusAdaptor::ListDebugTags(DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
  return ScopeLogger::GetInstance()->GetAllScopeNames();
}

map<string, DBus::Variant> ManagerDBusAdaptor::GetNetworksForGeolocation(
    DBus::Error& /*error*/) {  // NOLINT
  SLOG(this, 2) << __func__;
  map<string, DBus::Variant> networks;
  for (const auto& network : manager_->GetNetworksForGeolocation()) {
    Stringmaps value;
    // Convert GeolocationInfos to their Stringmaps equivalent.
    for (const auto& info : network.second) {
      value.push_back(info.properties());
    }
    networks[network.first] = StringmapsToVariant(value);
  }
  return networks;
}

bool ManagerDBusAdaptor::VerifyDestination(const string& certificate,
                                           const string& public_key,
                                           const string& nonce,
                                           const string& signed_data,
                                           const string& destination_udn,
                                           const string& hotspot_ssid,
                                           const string& hotspot_bssid,
                                           DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  DBus::Tag* tag = new DBus::Tag();
#if !defined(DISABLE_WIFI)
  Error e(Error::kOperationInitiated);
  manager_->VerifyDestination(certificate, public_key, nonce,
                              signed_data, destination_udn,
                              hotspot_ssid, hotspot_bssid,
                              GetBoolMethodReplyCallback(tag), &e);
#else
  Error e(Error::kNotImplemented);
#endif  // DISABLE_WIFI
  ReturnResultOrDefer(tag, e, &error);
  CHECK(e.IsFailure()) << __func__ << " should only return directly on error.";
  return false;
}

string ManagerDBusAdaptor::VerifyAndEncryptCredentials(
    const string& certificate,
    const string& public_key,
    const string& nonce,
    const string& signed_data,
    const string& destination_udn,
    const string& hotspot_ssid,
    const string& hotspot_bssid,
    const DBus::Path& network,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  DBus::Tag* tag = new DBus::Tag();
#if !defined(DISABLE_WIFI)
  Error e(Error::kOperationInitiated);
  manager_->VerifyAndEncryptCredentials(certificate, public_key, nonce,
                                        signed_data, destination_udn,
                                        hotspot_ssid, hotspot_bssid,
                                        network,
                                        GetStringMethodReplyCallback(tag),
                                        &e);
#else
  Error e(Error::kNotImplemented);
#endif  // DISABLE_WIFI
  ReturnResultOrDefer(tag, e, &error);
  CHECK(e.IsFailure()) << __func__ << " should only return directly on error.";
  return "";
}

string ManagerDBusAdaptor::VerifyAndEncryptData(
    const string& certificate,
    const string& public_key,
    const string& nonce,
    const string& signed_data,
    const string& destination_udn,
    const string& hotspot_ssid,
    const string& hotspot_bssid,
    const string& data,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  DBus::Tag* tag = new DBus::Tag();
#if !defined(DISABLE_WIFI)
  Error e(Error::kOperationInitiated);
  manager_->VerifyAndEncryptData(certificate, public_key, nonce,
                                 signed_data, destination_udn,
                                 hotspot_ssid, hotspot_bssid,
                                 data, GetStringMethodReplyCallback(tag),
                                 &e);
#else
  Error e(Error::kNotImplemented);
#endif  // DISABLE_WIFI
  ReturnResultOrDefer(tag, e, &error);
  CHECK(e.IsFailure()) << __func__ << " should only return directly on error.";
  return "";
}

void ManagerDBusAdaptor::ConnectToBestServices(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  manager_->ConnectToBestServices(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::CreateConnectivityReport(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  manager_->CreateConnectivityReport(&e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::ClaimInterface(const string& claimer_name,
                                        const string& interface_name,
                                        DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  manager_->ClaimDevice(claimer_name,
                        interface_name,
                        &e,
                        GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void ManagerDBusAdaptor::ReleaseInterface(const string& claimer_name,
                                          const string& interface_name,
                                          DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  manager_->ReleaseDevice(claimer_name, interface_name, &e);
  e.ToDBusError(&error);
}

void ManagerDBusAdaptor::SetSchedScan(const bool& enable,
                                      DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << enable;
  Error e;
  manager_->SetSchedScan(enable, &e);
  e.ToDBusError(&error);
}

}  // namespace shill

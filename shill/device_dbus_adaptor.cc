// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_dbus_adaptor.h"

#include <map>
#include <string>

#include "shill/device.h"
#include "shill/error.h"
#include "shill/logging.h"

using std::map;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(DeviceDBusAdaptor* d) { return d->GetRpcIdentifier(); }
}

// static
const char DeviceDBusAdaptor::kPath[] = "/device/";

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection* conn, Device* device)
    : DBusAdaptor(conn, kPath + SanitizePathElement(device->UniqueName())),
      device_(device),
      connection_name_(conn->unique_name()) {
}

DeviceDBusAdaptor::~DeviceDBusAdaptor() {
  device_ = nullptr;
}

const string& DeviceDBusAdaptor::GetRpcIdentifier() {
  return path();
}

const string& DeviceDBusAdaptor::GetRpcConnectionIdentifier() {
  return connection_name_;
}

void DeviceDBusAdaptor::EmitBoolChanged(const string& name, bool value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void DeviceDBusAdaptor::EmitUintChanged(const string& name,
                                        uint32_t value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void DeviceDBusAdaptor::EmitUint16Changed(const string& name, uint16_t value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Uint16ToVariant(value));
}

void DeviceDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void DeviceDBusAdaptor::EmitStringChanged(const string& name,
                                          const string& value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void DeviceDBusAdaptor::EmitStringmapChanged(const string& name,
                                             const Stringmap& value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::StringmapToVariant(value));
}

void DeviceDBusAdaptor::EmitStringmapsChanged(const string& name,
                                              const Stringmaps& value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::StringmapsToVariant(value));
}

void DeviceDBusAdaptor::EmitStringsChanged(const string& name,
                                              const Strings& value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::StringsToVariant(value));
}

void DeviceDBusAdaptor::EmitKeyValueStoreChanged(const string& name,
                                                 const KeyValueStore& value) {
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  PropertyChanged(name, DBusAdaptor::KeyValueStoreToVariant(value));
}

void DeviceDBusAdaptor::EmitRpcIdentifierChanged(const std::string& name,
                                                 const std::string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::PathToVariant(value));
}

void DeviceDBusAdaptor::EmitRpcIdentifierArrayChanged(
    const string& name,
    const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  vector<DBus::Path> paths;
  for (const auto& element : value) {
    paths.push_back(element);
  }

  PropertyChanged(name, DBusAdaptor::PathsToVariant(paths));
}

map<string, DBus::Variant> DeviceDBusAdaptor::GetProperties(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << " " << device_->UniqueName();
  map<string, DBus::Variant> properties;
  DBusAdaptor::GetProperties(device_->store(), &properties, &error);
  return properties;
}

void DeviceDBusAdaptor::SetProperty(const string& name,
                                    const DBus::Variant& value,
                                    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  DBusAdaptor::SetProperty(device_->mutable_store(), name, value, &error);
}

void DeviceDBusAdaptor::ClearProperty(const string& name,
                                      DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " " << name;
  DBusAdaptor::ClearProperty(device_->mutable_store(), name, &error);
}

void DeviceDBusAdaptor::Enable(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->SetEnabledPersistent(true, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::Disable(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->SetEnabledPersistent(false, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::ProposeScan(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e;
  // User scan requests, which are the likely source of DBus requests, probably
  // aren't time-critical so we might as well perform a complete scan.  It
  // also provides a failsafe for progressive scan.
  device_->Scan(Device::kFullScan, &e, __func__);
  e.ToDBusError(&error);
}

DBus::Path DeviceDBusAdaptor::AddIPConfig(const string& /*method*/,
                                          DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e(Error::kNotSupported, "This function is deprecated in shill");
  e.ToDBusError(&error);
  return "/";
}

void DeviceDBusAdaptor::Register(const string& network_id,
                                 DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << " (" << network_id << ")";
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->RegisterOnNetwork(network_id, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::RequirePin(
    const string& pin, const bool& require, DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->RequirePIN(pin, require, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::EnterPin(const string& pin,
                                 DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->EnterPIN(pin, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::UnblockPin(const string& unblock_code,
                                   const string& pin,
                                   DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->UnblockPIN(unblock_code, pin, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::ChangePin(const string& old_pin,
                                  const string& new_pin,
                                  DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->ChangePIN(old_pin, new_pin, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::Reset(DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->Reset(&e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

string DeviceDBusAdaptor::PerformTDLSOperation(const string& operation,
                                               const string& peer,
                                               DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName();
  Error e;
  string return_value = device_->PerformTDLSOperation(operation, peer, &e);
  e.ToDBusError(&error);
  return return_value;
}

void DeviceDBusAdaptor::ResetByteCounters(DBus::Error& error) {  // NOLINT
  device_->ResetByteCounters();
}

void DeviceDBusAdaptor::SetCarrier(const string& carrier,
                                   DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": Device " << device_->UniqueName()
                << "(" << carrier << ")";
  Error e(Error::kOperationInitiated);
  DBus::Tag* tag = new DBus::Tag();
  device_->SetCarrier(carrier, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::AddWakeOnPacketConnection(
    const string& ip_endpoint,
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  device_->AddWakeOnPacketConnection(ip_endpoint, &e);
  e.ToDBusError(&error);
}

void DeviceDBusAdaptor::RemoveWakeOnPacketConnection(
    const string& ip_endpoint, DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  device_->RemoveWakeOnPacketConnection(ip_endpoint, &e);
  e.ToDBusError(&error);
}

void DeviceDBusAdaptor::RemoveAllWakeOnPacketConnections(
    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__;
  Error e;
  device_->RemoveAllWakeOnPacketConnections(&e);
  e.ToDBusError(&error);
}

void DeviceDBusAdaptor::RequestRoam(const string& addr,
                                    DBus::Error& error) {  // NOLINT
  SLOG(this, 2) << __func__ << ": " << addr;
  Error e;
  device_->RequestRoam(addr, &e);
  e.ToDBusError(&error);
}

}  // namespace shill

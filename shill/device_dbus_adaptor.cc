// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_dbus_adaptor.h"

#include <map>
#include <string>

#include <base/bind.h>

#include "shill/device.h"
#include "shill/error.h"
#include "shill/scope_logger.h"

using base::Bind;
using std::map;
using std::string;

namespace shill {

// static
const char DeviceDBusAdaptor::kPath[] = "/device/";

DeviceDBusAdaptor::DeviceDBusAdaptor(DBus::Connection* conn, Device *device)
    : DBusAdaptor(conn, kPath + device->UniqueName()),
      device_(device),
      connection_name_(conn->unique_name()) {
}

DeviceDBusAdaptor::~DeviceDBusAdaptor() {
  device_ = NULL;
}
const std::string &DeviceDBusAdaptor::GetRpcIdentifier() {
  return path();
}

const std::string &DeviceDBusAdaptor::GetRpcConnectionIdentifier() {
  return connection_name_;
}

void DeviceDBusAdaptor::UpdateEnabled() {}

void DeviceDBusAdaptor::EmitBoolChanged(const std::string& name, bool value) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::BoolToVariant(value));
}

void DeviceDBusAdaptor::EmitUintChanged(const std::string& name, uint32 value) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Uint32ToVariant(value));
}

void DeviceDBusAdaptor::EmitIntChanged(const std::string& name, int value) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::Int32ToVariant(value));
}

void DeviceDBusAdaptor::EmitStringChanged(const std::string& name,
                                          const std::string& value) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringToVariant(value));
}

void DeviceDBusAdaptor::EmitStringmapsChanged(const std::string &name,
                                              const Stringmaps &value) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::StringmapsToVariant(value));
}

void DeviceDBusAdaptor::EmitKeyValueStoreChanged(const std::string &name,
                                                 const KeyValueStore &value) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  PropertyChanged(name, DBusAdaptor::KeyValueStoreToVariant(value));
}

map<string, ::DBus::Variant> DeviceDBusAdaptor::GetProperties(
    ::DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  map<string, ::DBus::Variant> properties;
  DBusAdaptor::GetProperties(device_->store(), &properties, &error);
  return properties;
}

void DeviceDBusAdaptor::SetProperty(const string &name,
                                    const ::DBus::Variant &value,
                                    ::DBus::Error &error) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  DBusAdaptor::SetProperty(device_->mutable_store(), name, value, &error);
}

void DeviceDBusAdaptor::ClearProperty(const std::string &name,
                                      ::DBus::Error &error) {
  SLOG(DBus, 2) << __func__ << ": " << name;
  DBusAdaptor::ClearProperty(device_->mutable_store(), name, &error);
}

void DeviceDBusAdaptor::Enable(::DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  device_->SetEnabledPersistent(true, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::Disable(::DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  device_->SetEnabledPersistent(false, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::ProposeScan(::DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e;
  device_->Scan(&e);
  e.ToDBusError(&error);
}

::DBus::Path DeviceDBusAdaptor::AddIPConfig(const string& ,
                                            ::DBus::Error &/*error*/) {
  SLOG(DBus, 2) << __func__;
  return "/";
}

void DeviceDBusAdaptor::Register(const string &network_id,
                                 ::DBus::Error &error) {
  SLOG(DBus, 2) << __func__ << "(" << network_id << ")";
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  device_->RegisterOnNetwork(network_id, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::RequirePin(
    const string &pin, const bool &require, DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  device_->RequirePIN(pin, require, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::EnterPin(const string &pin, DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  device_->EnterPIN(pin, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::UnblockPin(
    const string &unblock_code, const string &pin, DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e(Error::kOperationInitiated);
  DBus::Tag *tag = new DBus::Tag();
  device_->UnblockPIN(unblock_code, pin, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

void DeviceDBusAdaptor::ChangePin(
    const string &old_pin, const string &new_pin, DBus::Error &error) {
  SLOG(DBus, 2) << __func__;
  Error e;
  DBus::Tag *tag = new DBus::Tag();
  device_->ChangePIN(old_pin, new_pin, &e, GetMethodReplyCallback(tag));
  ReturnResultOrDefer(tag, e, &error);
}

}  // namespace shill

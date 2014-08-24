// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/exported_property_set.h"

#include <base/bind.h>
#include <dbus/bus.h>
#include <dbus/property.h>  // For kPropertyInterface

#include "chromeos/async_event_sequencer.h"
#include "chromeos/dbus/dbus_object.h"
#include "chromeos/dbus_utils.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace chromeos {

namespace dbus_utils {

namespace {
const char kExportFailedMessage[] = "Failed to register DBus method.";
}  // namespace

ExportedPropertySet::ExportedPropertySet(dbus::Bus* bus)
    : bus_(bus),
      weak_ptr_factory_(this) { }

ExportedPropertySet::PropertyWriter ExportedPropertySet::GetPropertyWriter(
    const std::string& interface_name) {
  return base::Bind(&ExportedPropertySet::WritePropertiesDictToMessage,
                    weak_ptr_factory_.GetWeakPtr(),
                    interface_name);
}

void ExportedPropertySet::RegisterProperty(
    const std::string& interface_name,
    const std::string& property_name,
    ExportedPropertyBase* exported_property) {
  bus_->AssertOnOriginThread();
  auto& prop_map = properties_[interface_name];
  auto res = prop_map.insert(std::make_pair(property_name, exported_property));
  CHECK(res.second) << "Property '" << property_name << "' already exists";
  // Technically, the property set exists longer than the properties themselves,
  // so we could use Unretained here rather than a weak pointer.
  ExportedPropertyBase::OnUpdateCallback cb = base::Bind(
      &ExportedPropertySet::HandlePropertyUpdated,
      weak_ptr_factory_.GetWeakPtr(),
      interface_name, property_name);
  exported_property->SetUpdateCallback(cb);
}

std::unique_ptr<dbus::Response> ExportedPropertySet::HandleGetAll(
    dbus::MethodCall* method_call,
    const std::string& interface_name) {
  bus_->AssertOnOriginThread();
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call).release());
  dbus::MessageWriter resp_writer(response.get());
  WritePropertiesDictToMessage(interface_name, &resp_writer);
  return response;
}

void ExportedPropertySet::WritePropertiesDictToMessage(
    const std::string& interface_name,
    dbus::MessageWriter* writer) {
  dbus::MessageWriter dict_writer(nullptr);
  writer->OpenArray("{sv}", &dict_writer);
  auto property_map_itr = properties_.find(interface_name);
  if (property_map_itr != properties_.end()) {
    for (const auto& kv : property_map_itr->second) {
      dbus::MessageWriter entry_writer(nullptr);
      dict_writer.OpenDictEntry(&entry_writer);
      entry_writer.AppendString(kv.first);
      kv.second->AppendValueToWriter(&entry_writer);
      dict_writer.CloseContainer(&entry_writer);
    }
  } else {
    LOG(WARNING) << "No properties found for interface interface_name";
  }
  writer->CloseContainer(&dict_writer);
}

std::unique_ptr<dbus::Response> ExportedPropertySet::HandleGet(
    dbus::MethodCall* method_call,
    const std::string& interface_name,
    const std::string& property_name) {
  bus_->AssertOnOriginThread();
  auto property_map_itr = properties_.find(interface_name);
  if (property_map_itr == properties_.end()) {
    return CreateDBusErrorResponse(method_call,
                                   DBUS_ERROR_INVALID_ARGS,
                                   "No such interface on object.");
  }
  LOG(INFO) << "Looking for " << property_name << " on " << interface_name;
  auto property_itr = property_map_itr->second.find(property_name);
  if (property_itr == property_map_itr->second.end()) {
    return CreateDBusErrorResponse(method_call,
                                   DBUS_ERROR_INVALID_ARGS,
                                   "No such property on interface.");
  }
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call).release());
  dbus::MessageWriter resp_writer(response.get());
  property_itr->second->AppendValueToWriter(&resp_writer);
  return response;
}

std::unique_ptr<dbus::Response> ExportedPropertySet::HandleSet(
    dbus::MethodCall* method_call) {
  bus_->AssertOnOriginThread();
  return CreateDBusErrorResponse(method_call,
                                 DBUS_ERROR_NOT_SUPPORTED,
                                 "Method Set is not supported.");
}

void ExportedPropertySet::HandlePropertyUpdated(
    const std::string& interface_name,
    const std::string& property_name,
    const ExportedPropertyBase* exported_property) {
  bus_->AssertOnOriginThread();
  // Send signal only if the object has been exported successfully.
  if (!exported_object_)
    return;
  dbus::Signal signal(dbus::kPropertiesInterface, dbus::kPropertiesChanged);
  dbus::MessageWriter writer(&signal);
  dbus::MessageWriter array_writer(nullptr);
  dbus::MessageWriter dict_writer(nullptr);
  writer.AppendString(interface_name);
  writer.OpenArray("{sv}", &array_writer);
  array_writer.OpenDictEntry(&dict_writer);
  dict_writer.AppendString(property_name);
  exported_property->AppendValueToWriter(&dict_writer);
  array_writer.CloseContainer(&dict_writer);
  writer.CloseContainer(&array_writer);
  // The interface specification tells us to include this list of properties
  // which have changed, but for whom no value is conveyed.  Currently, we
  // don't do anything interesting here.
  writer.OpenArray("s", &array_writer);
  writer.CloseContainer(&array_writer);
  // This sends the signal asynchronously.  However, the raw message inside
  // the signal object is ref-counted, so we're fine to allocate the Signal
  // object on our local stack.
  exported_object_->SendSignal(&signal);
}

void ExportedPropertyBase::NotifyPropertyChanged() {
  // These is a brief period after the construction of an ExportedProperty
  // when this callback is not initialized because the property has not
  // been registered with the parent ExportedPropertySet.  During this period
  // users should be initializing values via SetValue, and no notifications
  // should be triggered by the ExportedPropertySet.
  if (!on_update_callback_.is_null()) {
    on_update_callback_.Run(this);
  }
}

void ExportedPropertyBase::SetUpdateCallback(const OnUpdateCallback& cb) {
  on_update_callback_ = cb;
}

}  // namespace dbus_utils

}  // namespace chromeos

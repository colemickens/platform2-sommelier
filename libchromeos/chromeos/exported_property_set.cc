// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/exported_property_set.h"

#include <base/bind.h>
#include <dbus/bus.h>
#include <dbus/property.h>  // For kPropertyInterface

#include "chromeos/any.h"
#include "chromeos/async_event_sequencer.h"
#include "chromeos/dbus/dbus_object.h"
#include "chromeos/dbus_utils.h"

using chromeos::dbus_utils::AsyncEventSequencer;

namespace chromeos {

namespace dbus_utils {

ExportedPropertySet::ExportedPropertySet(dbus::Bus* bus)
    : bus_(bus),
      weak_ptr_factory_(this) { }

ExportedPropertySet::PropertyWriter ExportedPropertySet::GetPropertyWriter(
    const std::string& interface_name) {
  return base::Bind(&ExportedPropertySet::WritePropertiesToDict,
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

dbus_utils::Dictionary ExportedPropertySet::HandleGetAll(
    chromeos::ErrorPtr* error,
    const std::string& interface_name) {
  bus_->AssertOnOriginThread();
  return GetInterfaceProperties(interface_name);
}

dbus_utils::Dictionary ExportedPropertySet::GetInterfaceProperties(
     const std::string& interface_name) const {
  dbus_utils::Dictionary properties;
  auto property_map_itr = properties_.find(interface_name);
  if (property_map_itr != properties_.end()) {
    for (const auto& kv : property_map_itr->second)
      properties.insert(std::make_pair(kv.first, kv.second->GetValue()));
  }
  return properties;
}

void ExportedPropertySet::WritePropertiesToDict(
    const std::string& interface_name,
    dbus_utils::Dictionary* dict) {
  *dict = GetInterfaceProperties(interface_name);
}

chromeos::Any ExportedPropertySet::HandleGet(
    chromeos::ErrorPtr* error,
    const std::string& interface_name,
    const std::string& property_name) {
  bus_->AssertOnOriginThread();
  auto property_map_itr = properties_.find(interface_name);
  if (property_map_itr == properties_.end()) {
    chromeos::Error::AddTo(error,
                           dbus_utils::kDBusErrorDomain,
                           DBUS_ERROR_INVALID_ARGS,
                           "No such interface on object.");
    return chromeos::Any();
  }
  LOG(INFO) << "Looking for " << property_name << " on " << interface_name;
  auto property_itr = property_map_itr->second.find(property_name);
  if (property_itr == property_map_itr->second.end()) {
    chromeos::Error::AddTo(error,
                           dbus_utils::kDBusErrorDomain,
                           DBUS_ERROR_INVALID_ARGS,
                           "No such property on interface.");
    return chromeos::Any();
  }
  return property_itr->second->GetValue();
}

void ExportedPropertySet::HandleSet(
    chromeos::ErrorPtr* error,
    const std::string& interface_name,
    const std::string& property_name,
    const chromeos::Any& value) {
  bus_->AssertOnOriginThread();
  chromeos::Error::AddTo(error,
                         dbus_utils::kDBusErrorDomain,
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
  writer.AppendString(interface_name);
  dbus_utils::Dictionary changed_properties{
      {property_name, exported_property->GetValue()}
  };
  dbus_utils::AppendValueToWriter(&writer, changed_properties);
  // The interface specification tells us to include this list of properties
  // which have changed, but for whom no value is conveyed.  Currently, we
  // don't do anything interesting here.
  std::vector<std::string> invalidated_properties;  // empty.
  dbus_utils::AppendValueToWriter(&writer, invalidated_properties);
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

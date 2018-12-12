// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/exported_object_manager_wrapper.h"

#include <memory>
#include <set>
#include <utility>

#include <base/stl_util.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/object_manager.h>

namespace {

// Called when ObjectManager is exported.
void OnExportedObjectManagerRegistered(bool success) {
  if (!success)
    LOG(ERROR) << "Failed to export object manager";
}

// Called when a D-Bus object is exported.
void OnObjectExported(std::string object_path, bool success) {
  VLOG(1) << "Completed exported object registration " << object_path
          << ", success = " << success;
}

// Called when a D-Bus interface is unexported.
void OnInterfaceUnexported(std::string interface_name, bool success) {
  VLOG(1) << "Completed unexporting interface " << interface_name
          << ", success = " << success;
  if (!success)
    LOG(ERROR) << "Failed unexporting interface " << interface_name;
}

}  // namespace

namespace bluetooth {

ExportedInterface::ExportedInterface(
    const dbus::ObjectPath& object_path,
    const std::string& interface_name,
    brillo::dbus_utils::DBusObject* dbus_object)
    : object_path_(object_path),
      interface_name_(interface_name),
      dbus_object_(dbus_object) {
  dbus_object->AddOrGetInterface(interface_name);
}

void ExportedInterface::ExportAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& callback) {
  dbus_object_->ExportInterfaceAsync(interface_name_, callback);
  is_exported_ = true;
}

void ExportedInterface::Unexport() {
  std::set<std::string> exported_property_names;
  for (const auto& kv : exported_properties_) {
    exported_property_names.insert(kv.first);
  }

  for (const std::string& property_name : exported_property_names) {
    EnsureExportedPropertyUnregistered(property_name);
  }

  // Unexport before removing the interface to make sure the method handlers are
  // unregistered.
  dbus_object_->UnexportInterfaceAsync(
      interface_name_, base::Bind(&OnInterfaceUnexported, interface_name_));
  dbus_object_->RemoveInterface(interface_name_);
  is_exported_ = false;
}

void ExportedInterface::AddRawMethodHandler(
    const std::string& method_name,
    const base::Callback<void(dbus::MethodCall*,
                              dbus::ExportedObject::ResponseSender)>& handler) {
  dbus_object_->AddOrGetInterface(interface_name_)
      ->AddRawMethodHandler(method_name, handler);
}

void ExportedInterface::SyncPropertyToExportedProperty(
    const std::string& property_name,
    dbus::PropertyBase* property_base,
    PropertyFactoryBase* property_factory) {
  if (!property_base->is_valid()) {
    EnsureExportedPropertyUnregistered(property_name);
    return;
  }

  brillo::dbus_utils::ExportedPropertyBase* exported_property_base =
      EnsureExportedPropertyRegistered(property_name, property_factory);
  property_factory->CopyPropertyToExportedProperty(property_base,
                                                   exported_property_base);
}

brillo::dbus_utils::ExportedPropertyBase*
ExportedInterface::EnsureExportedPropertyRegistered(
    const std::string& property_name, PropertyFactoryBase* property_factory) {
  auto iter = exported_properties_.find(property_name);
  if (iter != exported_properties_.end())
    return iter->second.get();

  VLOG(2) << "Adding property " << property_name << " to exported object "
          << object_path_.value() << " on interface " << interface_name_;

  auto ret = exported_properties_.emplace(
      property_name, property_factory->CreateExportedProperty());
  brillo::dbus_utils::ExportedPropertyBase* exported_property =
      ret.first->second.get();
  dbus_object_->FindInterface(interface_name_)
      ->AddProperty(property_name, exported_property);
  return exported_property;
}

void ExportedInterface::EnsureExportedPropertyUnregistered(
    const std::string& property_name) {
  if (!base::ContainsKey(exported_properties_, property_name))
    return;

  VLOG(2) << "Removing property " << property_name << " to exported object "
          << object_path_.value() << " on interface " << interface_name_;

  dbus_object_->FindInterface(interface_name_)->RemoveProperty(property_name);
  exported_properties_.erase(property_name);
}

brillo::dbus_utils::ExportedPropertyBase*
ExportedInterface::GetRegisteredExportedProperty(
    const std::string& property_name) {
  if (!base::ContainsKey(exported_properties_, property_name))
    return nullptr;

  return exported_properties_[property_name].get();
}
////////////////////////////////////////////////////////////////////////////////

ExportedObject::ExportedObject(
    brillo::dbus_utils::ExportedObjectManager* exported_object_manager,
    const scoped_refptr<dbus::Bus>& bus,
    const dbus::ObjectPath& object_path,
    brillo::dbus_utils::DBusObject::PropertyHandlerSetupCallback
        property_handler_setup_callback)
    : object_path_(object_path),
      dbus_object_(exported_object_manager,
                   bus,
                   object_path,
                   property_handler_setup_callback) {}

ExportedObject::~ExportedObject() {
  if (is_registered_)
    dbus_object_.UnregisterAsync();
}

ExportedInterface* ExportedObject::GetExportedInterface(
    const std::string& interface_name) {
  if (!base::ContainsKey(exported_interfaces_, interface_name))
    return nullptr;

  return exported_interfaces_.find(interface_name)->second.get();
}

void ExportedObject::AddExportedInterface(const std::string& interface_name) {
  CHECK(!base::ContainsKey(exported_interfaces_, interface_name))
      << "Interface " << interface_name << " has been added before";

  exported_interfaces_.emplace(
      interface_name, std::make_unique<ExportedInterface>(
                          object_path_, interface_name, &dbus_object_));
}

void ExportedObject::RemoveExportedInterface(
    const std::string& interface_name) {
  auto iter = exported_interfaces_.find(interface_name);
  CHECK(iter != exported_interfaces_.end())
      << "Interface " << interface_name << " has not been added before";

  iter->second->Unexport();
  exported_interfaces_.erase(iter);
}

void ExportedObject::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& callback) {
  CHECK(!is_registered_) << "Object " << object_path_.value()
                         << " has been registered before";
  is_registered_ = true;
  dbus_object_.RegisterAsync(callback);
}

////////////////////////////////////////////////////////////////////////////////

ExportedObjectManagerWrapper::ExportedObjectManagerWrapper(
    scoped_refptr<dbus::Bus> bus,
    std::unique_ptr<brillo::dbus_utils::ExportedObjectManager>
        exported_object_manager)
    : bus_(bus),
      exported_object_manager_(std::move(exported_object_manager)),
      weak_ptr_factory_(this) {
  exported_object_manager_->RegisterAsync(
      base::Bind(&OnExportedObjectManagerRegistered));
}

void ExportedObjectManagerWrapper::SetPropertyHandlerSetupCallback(
    const brillo::dbus_utils::DBusObject::PropertyHandlerSetupCallback&
        callback) {
  property_handler_setup_callback_ = callback;
}

void ExportedObjectManagerWrapper::AddExportedInterface(
    const dbus::ObjectPath& object_path, const std::string& interface_name) {
  EnsureExportedObjectRegistered(object_path);
  GetExportedObject(object_path)->AddExportedInterface(interface_name);
}

void ExportedObjectManagerWrapper::RemoveExportedInterface(
    const dbus::ObjectPath& object_path, const std::string& interface_name) {
  ExportedObject* exported_object = GetExportedObject(object_path);
  if (!exported_object) {
    LOG(WARNING) << "Object " << object_path.value()
                 << " hasn't been added before";
    return;
  }

  exported_object->RemoveExportedInterface(interface_name);

  if (exported_object->exported_interfaces_.empty()) {
    // If the exported object has no more exported interfaces, unregister the
    // object. Deleting the ExportedObject will take care of
    // unregistering this object from the exporting service.
    VLOG(1) << "Deleting exported object " << object_path.value();
    exported_objects_.erase(object_path.value());
  }
}

ExportedInterface* ExportedObjectManagerWrapper::GetExportedInterface(
    const dbus::ObjectPath& object_path, const std::string& interface_name) {
  ExportedObject* exported_object = GetExportedObject(object_path);
  if (!exported_object)
    return nullptr;

  return exported_object->GetExportedInterface(interface_name);
}

void ExportedObjectManagerWrapper::EnsureExportedObjectRegistered(
    const dbus::ObjectPath& object_path) {
  if (base::ContainsKey(exported_objects_, object_path.value()))
    return;

  VLOG(1) << "Adding new ExportedObject " << object_path.value();
  auto exported_object = std::make_unique<ExportedObject>(
      exported_object_manager_.get(), bus_, object_path,
      property_handler_setup_callback_);
  exported_object->RegisterAsync(
      base::Bind(&OnObjectExported, object_path.value()));
  exported_objects_.emplace(object_path.value(), std::move(exported_object));
}

ExportedObject* ExportedObjectManagerWrapper::GetExportedObject(
    const dbus::ObjectPath& object_path) {
  auto iter = exported_objects_.find(object_path.value());
  if (iter == exported_objects_.end())
    return nullptr;
  return iter->second.get();
}

}  // namespace bluetooth

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/impersonation_object_manager_interface.h"

#include <utility>

#include <brillo/bind_lambda.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/object_manager.h>

namespace {

// Called when an interface of a D-Bus object is exported.
void OnInterfaceExported(std::string object_path,
                         std::string interface_name,
                         bool success) {
  VLOG(1) << "Completed interface export " << interface_name << " of object "
          << object_path << ", success = " << success;
}

}  // namespace

namespace bluetooth {

ImpersonationObjectManagerInterface::ImpersonationObjectManagerInterface(
    const scoped_refptr<dbus::Bus>& bus,
    dbus::ObjectManager* object_manager,
    ExportedObjectManagerWrapper* exported_object_manager_wrapper,
    std::unique_ptr<InterfaceHandler> interface_handler,
    const std::string& interface_name)
    : ObjectManagerInterfaceMultiplexer(interface_name),
      bus_(bus),
      object_manager_(object_manager),
      exported_object_manager_wrapper_(exported_object_manager_wrapper),
      interface_handler_(std::move(interface_handler)),
      weak_ptr_factory_(this) {
  exported_object_manager_wrapper_->SetPropertyHandlerSetupCallback(base::Bind(
      &ImpersonationObjectManagerInterface::SetupPropertyMethodHandlers,
      weak_ptr_factory_.GetWeakPtr()));
}

dbus::PropertySet* ImpersonationObjectManagerInterface::CreateProperties(
    const std::string& service_name,
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  VLOG(1) << "Service " << service_name << " CreateProperties "
          << object_path.value() << " interface " << interface_name
          << " object proxy " << object_proxy;

  auto property_set = std::make_unique<PropertySet>(
      object_proxy, interface_name,
      base::Bind(&ImpersonationObjectManagerInterface::OnPropertyChanged,
                 weak_ptr_factory_.GetWeakPtr(), service_name, object_path,
                 interface_name));

  for (const auto& kv : *interface_handler_->GetPropertyFactoryMap())
    property_set->RegisterProperty(kv.first, kv.second->CreateProperty());

  // When CreateProperties is called that means the source service exports
  // interface |interface_name| on object |object_path|. So here we mimic
  // that to our exported object manager.
  exported_object_manager_wrapper_->AddExportedInterface(object_path,
                                                         interface_name);

  return property_set.release();
}

void ImpersonationObjectManagerInterface::ObjectAdded(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  VLOG(1) << "Service " << service_name << " added object "
          << object_path.value() << " on interface " << interface_name;
  // Whenever we detect that an interface has been added to the impersonated
  // service, we immediately export the same interface to the impersonating
  // service.
  ExportedInterface* exported_interface =
      exported_object_manager_wrapper_->GetExportedInterface(object_path,
                                                             interface_name);
  if (!exported_interface)
    return;
  exported_interface->ExportAsync(
      base::Bind(&OnInterfaceExported, object_path.value(), interface_name));
}

void ImpersonationObjectManagerInterface::ObjectRemoved(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  VLOG(1) << "Service " << service_name << " removed object "
          << object_path.value() << " on interface " << interface_name;
  // Whenever we detect that an interface has been removed from the impersonated
  // service, we immediately unexport the same interface from the impersonating
  // service.
  exported_object_manager_wrapper_->RemoveExportedInterface(object_path,
                                                            interface_name);
}

void ImpersonationObjectManagerInterface::OnPropertyChanged(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name,
    const std::string& property_name) {
  VLOG(2) << "Property " << property_name << " on interface " << interface_name
          << " of object " << object_path.value() << " changed.";

  PropertyFactoryBase* property_factory =
      interface_handler_->GetPropertyFactoryMap()
          ->find(property_name)
          ->second.get();

  // When property value change is detected from the impersonated service,
  // we immediately update the corresponding property of the impersonating
  // service.
  exported_object_manager_wrapper_
      ->GetExportedInterface(object_path, interface_name)
      ->CopyPropertyToExportedProperty(
          property_name,
          static_cast<PropertySet*>(
              object_manager_->GetProperties(object_path, interface_name))
              ->GetProperty(property_name),
          property_factory);
}

void ImpersonationObjectManagerInterface::HandlePropertiesChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  // Does nothing, needed only to suppress unhandled signal warning.
}

void ImpersonationObjectManagerInterface::SetupPropertyMethodHandlers(
    brillo::dbus_utils::DBusInterface* prop_interface,
    brillo::dbus_utils::ExportedPropertySet* property_set) {
  // Installs standard property handlers.
  // TODO(sonnysasaka): Add custom Set handler to forward the call to the source
  // service.
  prop_interface->AddSimpleMethodHandler(
      dbus::kPropertiesGetAll, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGetAll);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesGet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGet);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesSet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleSet);

  // Suppresses unhandled method warning by installing a no-op handler for
  // PropertiesChanged signals.
  prop_interface->AddRawMethodHandler(
      dbus::kPropertiesChanged, weak_ptr_factory_.GetWeakPtr(),
      &ImpersonationObjectManagerInterface::HandlePropertiesChanged);
}

}  // namespace bluetooth

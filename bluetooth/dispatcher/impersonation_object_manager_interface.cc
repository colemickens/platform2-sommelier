// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/dispatcher/impersonation_object_manager_interface.h"

#include <utility>

#include <base/stl_util.h>
#include <brillo/bind_lambda.h>
#include <brillo/dbus/exported_object_manager.h>
#include <dbus/dbus.h>
#include <dbus/object_manager.h>

#include "bluetooth/dispatcher/dbus_util.h"

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
    ExportedObjectManagerWrapper* exported_object_manager_wrapper,
    std::unique_ptr<InterfaceHandler> interface_handler,
    const std::string& interface_name,
    ClientManager* client_manager)
    : ObjectManagerInterfaceMultiplexer(interface_name),
      bus_(bus),
      exported_object_manager_wrapper_(exported_object_manager_wrapper),
      interface_handler_(std::move(interface_handler)),
      client_manager_(client_manager),
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

  for (const auto& kv : interface_handler_->GetPropertyFactoryMap())
    property_set->RegisterProperty(kv.first, kv.second->CreateProperty());

  // When CreateProperties is called that means the source service exports
  // interface |interface_name| on object |object_path|. So here we mimic
  // that to our exported object manager. However we skip adding exported
  // if another service has already exposed this object at this interface.
  if (!HasImpersonatedServicesForObject(object_path.value())) {
    exported_object_manager_wrapper_->AddExportedInterface(object_path,
                                                           interface_name);
  }
  AddImpersonatedServiceForObject(object_path.value(), service_name);

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
  if (!exported_interface || exported_interface->is_exported()) {
    // Skip exporting the interface if another service has triggered this
    // interface export.
    return;
  }

  // Export the methods that are defined by |interface_handler_|.
  // Any method call will be forwarded the the impersonated service via a
  // specific per-client D-Bus connection.
  for (const std::string& method_name : interface_handler_->GetMethodNames())
    exported_interface->AddRawMethodHandler(
        method_name, base::Bind(&ImpersonationObjectManagerInterface::
                                    HandleForwardMessageWithClientConnection,
                                weak_ptr_factory_.GetWeakPtr()));

  exported_interface->ExportAsync(
      base::Bind(&OnInterfaceExported, object_path.value(), interface_name));
}

void ImpersonationObjectManagerInterface::ObjectRemoved(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  VLOG(1) << "Service " << service_name << " removed object "
          << object_path.value() << " on interface " << interface_name;

  RemoveImpersonatedServiceForObject(object_path.value(), service_name);

  // Whenever we detect that an interface has been removed from the impersonated
  // service, we immediately unexport the same interface from the impersonating
  // service if this is the last service exposing this object at this interface.
  if (!HasImpersonatedServicesForObject(object_path.value())) {
    exported_object_manager_wrapper_->RemoveExportedInterface(object_path,
                                                              interface_name);
  } else {
    // One of the services removed this object, but there is still other
    // services exposing this object. Update all the property values to reflect
    // the properties of the other service's object.
    std::string service = GetDefaultServiceForObject(object_path.value());
    for (const auto& kv : interface_handler_->GetPropertyFactoryMap()) {
      std::string property_name = kv.first;
      OnPropertyChanged(service, object_path, interface_name, property_name);
    }
  }
}

bool ImpersonationObjectManagerInterface::HasImpersonatedServicesForObject(
    const std::string& object_path) const {
  return base::ContainsKey(impersonated_services_, object_path);
}

std::string ImpersonationObjectManagerInterface::GetDefaultServiceForObject(
    const std::string& object_path) const {
  CHECK(base::ContainsKey(impersonated_services_, object_path) &&
        !impersonated_services_.find(object_path)->second.empty());

  for (const std::string& service : service_names()) {
    if (base::ContainsKey(impersonated_services_.find(object_path)->second,
                          service))
      return service;
  }

  LOG(FATAL) << "Default service not found";
  return "";
}

void ImpersonationObjectManagerInterface::AddImpersonatedServiceForObject(
    const std::string& object_path, const std::string& service_name) {
  impersonated_services_[object_path].insert(service_name);
}

void ImpersonationObjectManagerInterface::RemoveImpersonatedServiceForObject(
    const std::string& object_path, const std::string& service_name) {
  impersonated_services_[object_path].erase(service_name);
  if (impersonated_services_[object_path].empty())
    impersonated_services_.erase(object_path);
}

dbus::ObjectManager* ImpersonationObjectManagerInterface::GetObjectManager(
    const std::string& service_name) {
  auto it = object_managers().find(service_name);
  CHECK(it != object_managers().end())
      << "ObjectManager of service " << service_name << " doesn't exist";
  return it->second;
}

void ImpersonationObjectManagerInterface::OnPropertyChanged(
    const std::string& service_name,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name,
    const std::string& property_name) {
  VLOG(2) << "Property " << property_name << " on interface " << interface_name
          << " of object " << object_path.value() << " changed.";

  // Ignore any changed property of non-default services.
  if (service_name != GetDefaultServiceForObject(object_path.value()))
    return;

  PropertyFactoryBase* property_factory =
      interface_handler_->GetPropertyFactoryMap()
          .find(property_name)
          ->second.get();

  // When property value change is detected from the impersonated service,
  // we immediately update the corresponding property of the impersonating
  // service.
  exported_object_manager_wrapper_
      ->GetExportedInterface(object_path, interface_name)
      ->SyncPropertyToExportedProperty(
          property_name,
          static_cast<PropertySet*>(
              GetObjectManager(service_name)
                  ->GetProperties(object_path, interface_name))
              ->GetProperty(property_name),
          property_factory);
}

void ImpersonationObjectManagerInterface::HandlePropertiesChanged(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  // Do nothing, needed only to suppress unhandled signal warning.
}

void ImpersonationObjectManagerInterface::HandleForwardMessage(
    scoped_refptr<dbus::Bus> bus,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  if (!HasImpersonatedServicesForObject(method_call->GetPath().value())) {
    LOG(WARNING) << "No destination to forward method "
                 << method_call->GetInterface() << "."
                 << method_call->GetMember() << " for object  "
                 << method_call->GetPath().value() << " on interface "
                 << interface_name();
    return;
  }

  std::string service_name =
      GetDefaultServiceForObject(method_call->GetPath().value());
  DBusUtil::ForwardMethodCall(bus, service_name, method_call, response_sender);
}

void ImpersonationObjectManagerInterface::
    HandleForwardMessageWithClientConnection(
        dbus::MethodCall* method_call,
        dbus::ExportedObject::ResponseSender response_sender) {
  VLOG(1) << "Method " << method_call->GetMember() << " called by "
          << method_call->GetSender();
  std::string client_address = method_call->GetSender();
  DispatcherClient* client = client_manager_->EnsureClientAdded(client_address);
  VLOG(1) << "client = " << client;
  HandleForwardMessage(client->GetClientBus(), method_call, response_sender);
}

void ImpersonationObjectManagerInterface::SetupPropertyMethodHandlers(
    brillo::dbus_utils::DBusInterface* prop_interface,
    brillo::dbus_utils::ExportedPropertySet* property_set) {
  // Install standard property handlers.
  prop_interface->AddSimpleMethodHandler(
      dbus::kPropertiesGetAll, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGetAll);
  prop_interface->AddSimpleMethodHandlerWithError(
      dbus::kPropertiesGet, base::Unretained(property_set),
      &brillo::dbus_utils::ExportedPropertySet::HandleGet);
  prop_interface->AddRawMethodHandler(
      dbus::kPropertiesSet,
      base::Bind(&ImpersonationObjectManagerInterface::HandleForwardMessage,
                 weak_ptr_factory_.GetWeakPtr(), bus_));

  // Suppress unhandled method warning by installing a no-op handler for
  // PropertiesChanged signals.
  prop_interface->AddRawMethodHandler(
      dbus::kPropertiesChanged, weak_ptr_factory_.GetWeakPtr(),
      &ImpersonationObjectManagerInterface::HandlePropertiesChanged);
}

}  // namespace bluetooth

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_IMPERSONATION_OBJECT_MANAGER_INTERFACE_H_
#define BLUETOOTH_DISPATCHER_IMPERSONATION_OBJECT_MANAGER_INTERFACE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <brillo/dbus/exported_object_manager.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_manager.h>

#include "bluetooth/dispatcher/exported_object_manager_wrapper.h"
#include "bluetooth/dispatcher/object_manager_interface_multiplexer.h"
#include "bluetooth/dispatcher/property.h"

namespace bluetooth {

// Clients of ImpersonationObjectManagerInterface should sub-class a
// InterfaceHandler and implement GetPropertyFactoryMap to
// specify the impersonated properties.
class InterfaceHandler {
 public:
  using PropertyFactoryMap =
      std::map<std::string, std::unique_ptr<PropertyFactoryBase>>;
  virtual ~InterfaceHandler() = default;

  // Returns a pointer of a map of
  // (std::string property_name, PropertyFactory<T> property_factory)
  // that describes what properties this interface has and what type for each
  // of the properties.
  // The returned pointer is owned by this object, not owned by clients.
  virtual const PropertyFactoryMap& GetPropertyFactoryMap() const = 0;
};

// Impersonates other services' object manager interface to another exported
// object manager. At this moment only 1 ObjectManager source is supported so
// technically it doesn't do multiplexing yet.
// TODO(sonnysasaka): Implement support for multiple ObjectManager sources.
class ImpersonationObjectManagerInterface
    : public ObjectManagerInterfaceMultiplexer {
 public:
  // Doesn't own |object_manager| and |exported_object_manager_wrapper|, so
  // clients should make sure that those pointers outlive this object.
  ImpersonationObjectManagerInterface(
      const scoped_refptr<dbus::Bus>& bus,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper,
      std::unique_ptr<InterfaceHandler> interface_handler,
      const std::string& interface_name);

  // CreateProperties, ObjectAdded, and ObjectRemoved are
  // ObjectManagerInterfaceMultiplexer overrides.
  // At this moment only 1 ObjectManager source is supported so |service_name|
  // parameter is always ignored.
  dbus::PropertySet* CreateProperties(
      const std::string& service_name,
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;
  void ObjectAdded(const std::string& service_name,
                   const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override;
  void ObjectRemoved(const std::string& service_name,
                     const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override;

 private:
  // Returns the ObjectManager of the service |service_name|. The returned
  // pointer is owned by |bus_|.
  dbus::ObjectManager* GetObjectManager(const std::string& service_name);

  // Called when there is a value change of a property on the impersonated
  // interface.
  void OnPropertyChanged(const std::string& service_name,
                         const dbus::ObjectPath& object_path,
                         const std::string& interface_name,
                         const std::string& property_name);

  // Handles PropertiesChanged signal of this object.
  void HandlePropertiesChanged(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Forwards any message to the impersonated service.
  void HandleForwardMessage(
      scoped_refptr<dbus::Bus> bus,
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Registers our custom GetAll/Get/Set method handlers.
  void SetupPropertyMethodHandlers(
      brillo::dbus_utils::DBusInterface* prop_interface,
      brillo::dbus_utils::ExportedPropertySet* property_set);

  scoped_refptr<dbus::Bus> bus_;

  // The destination object manager that impersonates the source.
  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  // Defines what properties are to be impersonated.
  std::unique_ptr<InterfaceHandler> interface_handler_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<ImpersonationObjectManagerInterface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImpersonationObjectManagerInterface);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_IMPERSONATION_OBJECT_MANAGER_INTERFACE_H_

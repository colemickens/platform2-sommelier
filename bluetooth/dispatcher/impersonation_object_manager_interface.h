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

#include <chromeos/dbus/service_constants.h>
#include <dbus/object_manager.h>

#include "bluetooth/common/exported_object_manager_wrapper.h"
#include "bluetooth/common/property.h"
#include "bluetooth/dispatcher/client_manager.h"
#include "bluetooth/dispatcher/dispatcher_client.h"
#include "bluetooth/dispatcher/object_manager_interface_multiplexer.h"

namespace bluetooth {

// When impersonating multiple services, this rule describes whether to export
// an object depending on which impersonated service exports it.
enum class ObjectExportRule {
  // Exports the object if any impersonated service exports it.
  ANY_SERVICE,
  // Exports the object if all impersonated services export it.
  ALL_SERVICES,
};

enum class ForwardingRule {
  // Forwards to default service only.
  FORWARD_DEFAULT,
  // Forwards to all services.
  FORWARD_ALL,
};

// Clients of ImpersonationObjectManagerInterface should sub-class a
// InterfaceHandler and implement GetPropertyFactoryMap to
// specify the impersonated properties.
class InterfaceHandler {
 public:
  using PropertyFactoryMap =
      std::map<std::string, std::unique_ptr<PropertyFactoryBase>>;
  virtual ~InterfaceHandler() = default;

  // Returns a map of
  // (std::string property_name, PropertyFactory<T> property_factory)
  // that describes what properties this interface has and what type for each
  // of the properties.
  virtual const PropertyFactoryMap& GetPropertyFactoryMap() const = 0;

  // Returns a list of method names that this interface exposes.
  virtual const std::map<std::string, ForwardingRule>& GetMethodForwardings()
      const = 0;

  // Returns the object export rule.
  virtual ObjectExportRule GetObjectExportRule() const = 0;
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
  // |client_manager| is not owned, must outlive this object.
  ImpersonationObjectManagerInterface(
      const scoped_refptr<dbus::Bus>& bus,
      ExportedObjectManagerWrapper* exported_object_manager_wrapper,
      std::unique_ptr<InterfaceHandler> interface_handler,
      const std::string& interface_name,
      ClientManager* client_manager);

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

  // Forwards any message to the impersonated service.
  void HandleForwardMessage(
      ForwardingRule forwarding_rule,
      scoped_refptr<dbus::Bus> bus,
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

 private:
  void TriggerPropertiesChanged(const std::string& service,
                                const dbus::ObjectPath& object_path,
                                const std::string& interface_name);

  bool ShouldInterfaceBeExported(const std::string& object_path) const;

  bool HasImpersonatedServicesForObject(const std::string& object_path) const;
  int GetImpersonatedServicesCountForObject(
      const std::string& object_path) const;
  void AddImpersonatedServiceForObject(const std::string& object_path,
                                       const std::string& service_name);
  void RemoveImpersonatedServiceForObject(const std::string& object_path,
                                          const std::string& service_name);
  bool HasImpersonatedServiceForObject(const std::string& object_path,
                                       const std::string& service_name);
  // Returns the default service that exports the object. The default is chosen
  // to be the one which is registered first via RegisterToObjectManager().
  std::string GetDefaultServiceForObject(const std::string& object_path) const;

  // Returns the ObjectManager of the service |service_name|. The returned
  // pointer is owned by |bus_|.
  dbus::ObjectManager* GetObjectManager(const std::string& service_name);

  // Called when there is a value change of a property on the impersonated
  // interface.
  void OnPropertyChanged(const std::string& service_name,
                         const dbus::ObjectPath& object_path,
                         const std::string& interface_name,
                         const std::string& property_name);

  // Forwards any message to the impersonated service, but uses a different
  // D-Bus connection specific per client (based on the sender address of
  // |method_call|).
  void HandleForwardMessageWithClientConnection(
      ForwardingRule forwarding_rule,
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Forwards a method to service |service_index|. Once the method return is
  // received, this will forward the same method to the next service, until
  // there is no more service or the last response contains error.
  void ForwardMessageToNextService(
      scoped_refptr<dbus::Bus> bus,
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender,
      int service_index,
      std::unique_ptr<dbus::Response> last_response);

  void SetupPropertyHandlers(
      brillo::dbus_utils::DBusInterface* prop_interface,
      brillo::dbus_utils::ExportedPropertySet* property_set);

  void HandleForwardSetProperty(
      scoped_refptr<dbus::Bus> bus,
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  scoped_refptr<dbus::Bus> bus_;

  // Keeps which services expose any object, in the form of map of
  // object_path -> set of service names.
  std::map<std::string, std::set<std::string>> impersonated_services_;

  // The destination object manager that impersonates the source.
  ExportedObjectManagerWrapper* exported_object_manager_wrapper_;

  // Defines what properties are to be impersonated.
  std::unique_ptr<InterfaceHandler> interface_handler_;

  // Keeps track of clients who have called the exposed methods.
  ClientManager* client_manager_;

  // Must come last so that weak pointers will be invalidated before other
  // members are destroyed.
  base::WeakPtrFactory<ImpersonationObjectManagerInterface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImpersonationObjectManagerInterface);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_IMPERSONATION_OBJECT_MANAGER_INTERFACE_H_

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_DISPATCHER_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_
#define BLUETOOTH_DISPATCHER_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <dbus/object_manager.h>
#include <dbus/property.h>

namespace bluetooth {

class ObjectManagerInterfaceMultiplexer;

// Forwards an ObjectManager::Interface to a multiplexer.
// ObjectManager::Interface can listen to ObjectManager events from only one
// D-Bus service. Since we need to listen to ObjectManager events from multiple
// services, we need this class as an adapter between dbus::ObjectManager and
// bluetooth::ObjectManagerInterfaceMultiplexer.
class ForwardingObjectManagerInterface : public dbus::ObjectManager::Interface {
 public:
  // |service_name|: The D-Bus service name to listen ObjectManager events from.
  // |interface_multiplexer|: The multiplexer to send all events to.
  // ObjectManagerInterfaceMultiplexer owns ForwardingObjectManagerInterface
  // so that guarantees that |interface_multiplexer| outlives this object.
  ForwardingObjectManagerInterface(
      const std::string& service_name,
      ObjectManagerInterfaceMultiplexer* interface_multiplexer);
  ~ForwardingObjectManagerInterface() override = default;

  // dbus::ObjectManager::Interface overrides
  dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;
  void ObjectAdded(const dbus::ObjectPath& object_path,
                   const std::string& interface_name) override;
  void ObjectRemoved(const dbus::ObjectPath& object_path,
                     const std::string& interface_name) override;

 private:
  std::string service_name_;

  ObjectManagerInterfaceMultiplexer* interface_multiplexer_;

  DISALLOW_COPY_AND_ASSIGN(ForwardingObjectManagerInterface);
};

// Multiplexes dbus::ObjectManager::Interface of multiple services.
// This is basically a dbus::ObjectManager::Interface that can listen to
// ObjectManager events from more than one D-Bus service.
class ObjectManagerInterfaceMultiplexer {
 public:
  explicit ObjectManagerInterfaceMultiplexer(const std::string& interface_name);
  virtual ~ObjectManagerInterfaceMultiplexer() = default;

  const std::string& interface_name() const { return interface_name_; }

  // Starts listening to ObjectManager events from |object_manager|.
  // |object_manager| is not owned and callers should make sure it outlives
  // this object.
  void RegisterToObjectManager(dbus::ObjectManager* object_manager,
                               const std::string& service_name);

  // Returns a map of service name -> dbus::ObjectManager* that have been
  // registered via |RegisterToObjectManager|.
  const std::map<std::string, dbus::ObjectManager*>& object_managers() const {
    return object_managers_;
  }
  // The list of service names, keeping the order based on registration via
  // RegisterToObjectManager(). The ordering is useful to determine the priority
  // of services in case the same object/interface is exported by more than one
  // service.
  const std::vector<std::string>& service_names() const {
    return service_names_;
  }

  // CreateProperties, ObjectAdded, and ObjectRemoved are like
  // dbus::ObjectManager::Interface methods, but also accepts |service_name|
  // parameter so it can tell which service the interface events come from.
  // Subclasses should implement these methods just like they do with
  // dbus::ObjectManager::Interface, but additionally |service_name| should be
  // used to distinguish which service the events come from.
  virtual dbus::PropertySet* CreateProperties(
      const std::string& service_name,
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) = 0;
  virtual void ObjectAdded(const std::string& service_name,
                           const dbus::ObjectPath& object_path,
                           const std::string& interface_name) = 0;
  virtual void ObjectRemoved(const std::string& service_name,
                             const dbus::ObjectPath& object_path,
                             const std::string& interface_name) = 0;

 private:
  friend class ObjectManagerInterfaceMultiplexerTest;

  // The D-Bus interface name this object is listening to.
  std::string interface_name_;

  // The dbus::ObjectManager::Interface forwarders.
  std::map<std::string, std::unique_ptr<ForwardingObjectManagerInterface>>
      object_manager_interfaces_;

  // The list of dbus::ObjectManager* corresponding to their service name.
  std::map<std::string, dbus::ObjectManager*> object_managers_;
  std::vector<std::string> service_names_;

  DISALLOW_COPY_AND_ASSIGN(ObjectManagerInterfaceMultiplexer);
};

}  // namespace bluetooth

#endif  // BLUETOOTH_DISPATCHER_OBJECT_MANAGER_INTERFACE_MULTIPLEXER_H_

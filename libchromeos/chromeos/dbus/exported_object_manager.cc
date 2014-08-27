// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/exported_object_manager.h>

#include <vector>

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/data_serialization.h>
#include <dbus/object_manager.h>

using chromeos::dbus_utils::AsyncEventSequencer;

namespace chromeos {

namespace dbus_utils {

ExportedObjectManager::ExportedObjectManager(scoped_refptr<dbus::Bus> bus,
                                             const dbus::ObjectPath& path)
    : bus_(bus), dbus_object_(nullptr, bus, path) {}

void ExportedObjectManager::RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback) {
  bus_->AssertOnOriginThread();
  DBusInterface* itf =
      dbus_object_.AddOrGetInterface(dbus::kObjectManagerInterface);
  itf->AddMethodHandler(dbus::kObjectManagerGetManagedObjects,
                        base::Unretained(this),
                        &ExportedObjectManager::HandleGetManagedObjects);
  dbus_object_.RegisterAsync(completion_callback);
}

void ExportedObjectManager::ClaimInterface(
    const dbus::ObjectPath& path,
    const std::string& interface_name,
    const ExportedPropertySet::PropertyWriter& property_writer) {
  bus_->AssertOnOriginThread();
  // We're sending signals that look like:
  //   org.freedesktop.DBus.ObjectManager.InterfacesAdded (
  //       OBJPATH object_path,
  //       DICT<STRING,DICT<STRING,VARIANT>> interfaces_and_properties);
  dbus_utils::Dictionary property_dict;
  property_writer.Run(&property_dict);
  std::map<std::string, dbus_utils::Dictionary> interfaces_and_properties{
      {interface_name, property_dict}
  };
  dbus::Signal signal(dbus::kObjectManagerInterface,
                      dbus::kObjectManagerInterfacesAdded);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendObjectPath(path);
  dbus_utils::AppendValueToWriter(&signal_writer, interfaces_and_properties);
  dbus_object_.SendSignal(&signal);
  registered_objects_[path][interface_name] = property_writer;
}

void ExportedObjectManager::ReleaseInterface(
    const dbus::ObjectPath& path, const std::string& interface_name) {
  bus_->AssertOnOriginThread();
  auto interfaces_for_path_itr = registered_objects_.find(path);
  CHECK(interfaces_for_path_itr != registered_objects_.end())
      << "Attempting to signal interface removal for path " << path.value()
      << " which was never registered.";
  auto& interfaces_for_path = interfaces_for_path_itr->second;
  auto property_for_interface_itr = interfaces_for_path.find(interface_name);
  CHECK(property_for_interface_itr != interfaces_for_path.end())
      << "Attempted to remove interface " << interface_name << " from "
      << path.value() << ", but this interface was never registered.";
  interfaces_for_path.erase(interface_name);
  if (interfaces_for_path.empty())
    registered_objects_.erase(path);

  // We're sending signals that look like:
  //   org.freedesktop.DBus.ObjectManager.InterfacesRemoved (
  //       OBJPATH object_path, ARRAY<STRING> interfaces);
  dbus::Signal signal(dbus::kObjectManagerInterface,
                      dbus::kObjectManagerInterfacesRemoved);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendObjectPath(path);
  dbus_utils::AppendValueToWriter(&signal_writer,
                                  std::vector<std::string>{interface_name});
  dbus_object_.SendSignal(&signal);
}

ExportedObjectManager::ObjectMap
    ExportedObjectManager::HandleGetManagedObjects(ErrorPtr* error) {
  // Implements the GetManagedObjects method:
  //
  // org.freedesktop.DBus.ObjectManager.GetManagedObjects (
  //     out DICT<OBJPATH,
  //              DICT<STRING,
  //                   DICT<STRING,VARIANT>>> )
  bus_->AssertOnOriginThread();
  ExportedObjectManager::ObjectMap objects;
  for (const auto path_pair : registered_objects_) {
    std::map<std::string, Dictionary>& interfaces = objects[path_pair.first];
    const InterfaceProperties& interface2properties = path_pair.second;
    for (const auto interface : interface2properties) {
      interface.second.Run(&interfaces[interface.first]);
    }
  }
  return objects;
}

}  //  namespace dbus_utils

}  //  namespace chromeos

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/exported_object_manager.h"

#include <chromeos/async_event_sequencer.h>
#include <dbus/object_manager.h>


using chromeos::dbus_utils::AsyncEventSequencer;

namespace chromeos {

namespace dbus_utils {

ExportedObjectManager::ExportedObjectManager(scoped_refptr<dbus::Bus> bus,
                                             const dbus::ObjectPath& path)
    : bus_(bus), exported_object_(bus->GetExportedObject(path)) {}

void ExportedObjectManager::Init(const OnInitFinish& cb) {
  bus_->AssertOnOriginThread();
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  exported_object_->ExportMethod(
      dbus::kObjectManagerInterface,
      dbus::kObjectManagerGetManagedObjects,
      base::Bind(&ExportedObjectManager::HandleGetManagedObjects,
                 AsWeakPtr()),
      sequencer->GetExportHandler(
          dbus::kObjectManagerInterface,
          dbus::kObjectManagerGetManagedObjects,
          "Failed exporting GetManagedObjects method of ObjectManager",
          false));
  sequencer->OnAllTasksCompletedCall({cb});
}

void ExportedObjectManager::ClaimInterface(
    const dbus::ObjectPath& path,
    const std::string& interface_name,
    const PropertyWriter& property_writer) {
  bus_->AssertOnOriginThread();
  // We're sending signals that look like:
  //   org.freedesktop.DBus.ObjectManager.InterfacesAdded (
  //       OBJPATH object_path,
  //       DICT<STRING,DICT<STRING,VARIANT>> interfaces_and_properties);
  dbus::Signal signal(dbus::kObjectManagerInterface,
                      dbus::kObjectManagerInterfacesAdded);
  dbus::MessageWriter signal_writer(&signal);
  dbus::MessageWriter all_interfaces(&signal);
  dbus::MessageWriter each_interface(&signal);
  signal_writer.AppendObjectPath(path);
  signal_writer.OpenArray("{sa{sv}}", &all_interfaces);
  all_interfaces.OpenDictEntry(&each_interface);
  each_interface.AppendString(interface_name);
  property_writer.Run(&each_interface);
  all_interfaces.CloseContainer(&each_interface);
  signal_writer.CloseContainer(&all_interfaces);
  exported_object_->SendSignal(&signal);
  registered_objects_[path][interface_name] = property_writer;
}

void ExportedObjectManager::ReleaseInterface(
    const dbus::ObjectPath& path, const std::string& interface_name) {
  bus_->AssertOnOriginThread();
  auto interfaces_for_path_itr = registered_objects_.find(path);
  CHECK(interfaces_for_path_itr != registered_objects_.end())
      << "Attempting to signal interface removal for path " << path.value()
      << " which was never registered.";
  auto interfaces_for_path = interfaces_for_path_itr->second;
  auto property_for_interface_itr = interfaces_for_path.find(interface_name);
  CHECK(property_for_interface_itr != interfaces_for_path.end())
      << "Attempted to remove interface " << interface_name << " from "
      << path.value() << ", but this interface was never registered.";
  interfaces_for_path.erase(interface_name);
  if (interfaces_for_path.size() < 1) {
    registered_objects_.erase(path);
  }
  // We're sending signals that look like:
  //   org.freedesktop.DBus.ObjectManager.InterfacesRemoved (
  //       OBJPATH object_path, ARRAY<STRING> interfaces);
  dbus::Signal signal(dbus::kObjectManagerInterface,
                      dbus::kObjectManagerInterfacesRemoved);
  dbus::MessageWriter signal_writer(&signal);
  signal_writer.AppendObjectPath(path);
  dbus::MessageWriter interface_writer(nullptr);
  signal_writer.OpenArray("s", &interface_writer);
  interface_writer.AppendString(interface_name);
  signal_writer.CloseContainer(&interface_writer);
  exported_object_->SendSignal(&signal);
}

void ExportedObjectManager::HandleGetManagedObjects(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) const {
  // Implements the GetManagedObjects method:
  //
  // org.freedesktop.DBus.ObjectManager.GetManagedObjects (
  //     out DICT<OBJPATH,
  //              DICT<STRING,
  //                   DICT<STRING,VARIANT>>> )
  bus_->AssertOnOriginThread();
  scoped_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter response_writer(response.get());
  dbus::MessageWriter all_object_paths(nullptr);
  dbus::MessageWriter each_object_path(nullptr);
  dbus::MessageWriter all_interfaces(nullptr);
  dbus::MessageWriter each_interface(nullptr);

  response_writer.OpenArray("{oa{sa{sv}}}", &all_object_paths);
  for (const auto path_pair : registered_objects_) {
    const dbus::ObjectPath& path = path_pair.first;
    const InterfaceProperties& interface2properties = path_pair.second;
    all_object_paths.OpenDictEntry(&each_object_path);
    each_object_path.AppendObjectPath(path);
    each_object_path.OpenArray("{sa{sv}}", &all_interfaces);
    for (const auto interface : interface2properties) {
      const std::string& interface_name = interface.first;
      const PropertyWriter& property_writer = interface.second;
      all_interfaces.OpenDictEntry(&each_interface);
      each_interface.AppendString(interface_name);
      property_writer.Run(&each_interface);
      all_interfaces.CloseContainer(&each_interface);
    }
    each_object_path.CloseContainer(&all_interfaces);
    all_object_paths.CloseContainer(&each_object_path);
  }
  response_writer.CloseContainer(&all_object_paths);
  response_sender.Run(response.Pass());
}

}  //  namespace dbus_utils

}  //  namespace chromeos

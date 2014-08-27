// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_EXPORTED_OBJECT_MANAGER_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_EXPORTED_OBJECT_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/any.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/dbus/exported_property_set.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_path.h>

namespace chromeos {

namespace dbus_utils {

// ExportedObjectManager is a delegate that implements the
// org.freedesktop.DBus.ObjectManager interface on behalf of another
// object. It handles sending signals when new interfaces are added.
//
// This class is very similar to the ExportedPropertySet class, except that
// it allows objects to expose an object manager interface rather than the
// properties interface.
//
//  Example usage:
//
//   class ExampleObjectManager {
//    public:
//     ExampleObjectManager(dbus::Bus* bus)
//         : object_manager_(bus, "/my/objects/path") { }
//
//     void RegisterAsync(const CompletionAction& cb) {
//          object_manager_.RegisterAsync(cb);
//     }
//     void ClaimInterface(const dbus::ObjectPath& path,
//                         const std::string& interface_name,
//                         const ExportedPropertySet::PropertyWriter& writer) {
//       object_manager_->ClaimInterface(...);
//     }
//     void ReleaseInterface(const dbus::ObjectPath& path,
//                           const std::string& interface_name) {
//       object_manager_->ReleaseInterface(...);
//     }
//
//    private:
//     ExportedObjectManager object_manager_;
//   };
//
//   class MyObjectClaimingAnInterface {
//    public:
//     MyObjectClaimingAnInterface(ExampleObjectManager* object_manager)
//       : object_manager_(object_manager) {}
//
//     void OnInitFinish(bool success) {
//       if (!success) { /* handle that */ }
//       object_manager_->ClaimInterface(
//           my_path_, my_interface_, my_properties_.GetWriter());
//     }
//
//    private:
//     struct Properties : public ExportedPropertySet {
//      public:
//       /* Lots of interesting properties. */
//     };
//
//     Properties my_properties_;
//     ExampleObjectManager* object_manager_;
//   };
class ExportedObjectManager
    : public base::SupportsWeakPtr<ExportedObjectManager> {
 public:
  using ObjectMap =
      std::map<dbus::ObjectPath, std::map<std::string, Dictionary>>;
  using InterfaceProperties =
      std::map<std::string, ExportedPropertySet::PropertyWriter>;

  ExportedObjectManager(scoped_refptr<dbus::Bus> bus,
                        const dbus::ObjectPath& path);

  // Registers methods implementing the ObjectManager interface on the object
  // exported on the path given in the constructor. Must be called on the
  // origin thread.
  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  // Trigger a signal that |path| has added an interface |interface_name|
  // with properties as given by |writer|.
  void ClaimInterface(const dbus::ObjectPath& path,
                      const std::string& interface_name,
                      const ExportedPropertySet::PropertyWriter& writer);

  // Trigger a signal that |path| has removed an interface |interface_name|.
  void ReleaseInterface(const dbus::ObjectPath& path,
                        const std::string& interface_name);

  const scoped_refptr<dbus::Bus>& GetBus() const { return bus_; }

 private:
  ObjectMap HandleGetManagedObjects(chromeos::ErrorPtr* error);

  scoped_refptr<dbus::Bus> bus_;
  chromeos::dbus_utils::DBusObject dbus_object_;
  // Tracks all objects currently known to the ExportedObjectManager.
  std::map<dbus::ObjectPath, InterfaceProperties> registered_objects_;

  using SignalInterfacesAdded =
      DBusSignal<dbus::ObjectPath, std::map<std::string, Dictionary>>;
  using SignalInterfacesRemoved =
      DBusSignal<dbus::ObjectPath, std::vector<std::string>>;

  std::weak_ptr<SignalInterfacesAdded> signal_itf_added_;
  std::weak_ptr<SignalInterfacesRemoved> signal_itf_removed_;

  friend class ExportedObjectManagerTest;
  DISALLOW_COPY_AND_ASSIGN(ExportedObjectManager);
};

}  //  namespace dbus_utils

}  //  namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_EXPORTED_OBJECT_MANAGER_H_

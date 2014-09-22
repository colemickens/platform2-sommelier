// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_LIBBUFFET_COMMAND_LISTENER_H_
#define BUFFET_LIBBUFFET_COMMAND_LISTENER_H_

#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>
#include <dbus/object_manager.h>

#include "libbuffet/export.h"

namespace buffet {

class CommandPropertySet;
class Command;
// Have to use scoped_ptr<> instead of unique_ptr<> for callback parameter
// because base::Callback cannot handle C++11 move-only types.
using OnBuffetCommandCallback = base::Callback<void(scoped_ptr<Command>)>;

// buffet::CommandListener is a helper class that connects to Buffet's D-Bus
// object manager and listens to InterfacesAdded notifications. When a new
// Command D-Bus object becomes available, this class calls
// a OnBuffetCommandCallback with buffet::Command object instance that is
// a proxy for the remote D-Bus command object.
class LIBBUFFET_EXPORT CommandListener : public dbus::ObjectManager::Interface {
 public:
  CommandListener() = default;

  // Initializes the object and establishes connection to Buffet's  D-Bus Object
  // Manager. Callback |on_buffet_command_callback| is called with Command
  // object whenever a new Buffet command becomes available.
  bool Init(dbus::Bus* bus,
            const OnBuffetCommandCallback& on_buffet_command_callback);

 protected:
  // Callback invoked when the value of property |property_name| of an object
  // at |object_path| is changed.
  virtual void OnPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name);

  // Called by D-Bus ObjectManager to notify that an object has been added with
  // the path |object_path|.
  void ObjectAdded(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

  // Called by D-Bus ObjectManager to notify that an object with the path
  // |object_path| has been removed.
  void ObjectRemoved(
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

 private:
  // Override from dbus::ObjectManager::Interface to create the property set
  // for out Command D-Bus object.
  LIBBUFFET_PRIVATE dbus::PropertySet* CreateProperties(
      dbus::ObjectProxy* object_proxy,
      const dbus::ObjectPath& object_path,
      const std::string& interface_name) override;

  // Gets the CommandPropertySet for the command object at |object_path|.
  LIBBUFFET_PRIVATE CommandPropertySet* GetProperties(
      const dbus::ObjectPath& object_path) const;

  // Gets the D-Bus proxy for the command object at |object_path|.
  LIBBUFFET_PRIVATE dbus::ObjectProxy* GetObjectProxy(
    const dbus::ObjectPath& object_path) const;

  dbus::ObjectManager* object_manager_;
  OnBuffetCommandCallback on_buffet_command_callback_;
  base::WeakPtrFactory<CommandListener> weak_ptr_factory_{this};

  friend class Command;
  DISALLOW_COPY_AND_ASSIGN(CommandListener);
};

}  // namespace buffet

#endif  // BUFFET_LIBBUFFET_COMMAND_LISTENER_H_

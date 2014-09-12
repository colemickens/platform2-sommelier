// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libbuffet/command_listener.h"

#include <dbus/bus.h>

#include "libbuffet/command.h"
#include "libbuffet/dbus_constants.h"
#include "libbuffet/private/command_property_set.h"

namespace buffet {

CommandPropertySet::CommandPropertySet(dbus::ObjectProxy* object_proxy,
                                       const std::string& interface_name,
                                       const PropertyChangedCallback& callback)
    : dbus::PropertySet(object_proxy, interface_name, callback) {
  RegisterProperty(dbus_constants::kCommandName, &name);
  RegisterProperty(dbus_constants::kCommandCategory, &category);
  RegisterProperty(dbus_constants::kCommandId, &id);
  RegisterProperty(dbus_constants::kCommandStatus, &status);
  RegisterProperty(dbus_constants::kCommandProgress, &progress);
  RegisterProperty(dbus_constants::kCommandParameters, &parameters);
}

bool CommandListener::Init(
    dbus::Bus* bus, const OnBuffetCommandCallback& on_buffet_command_callback) {
  object_manager_ = bus->GetObjectManager(
      dbus_constants::kServiceName,
      dbus::ObjectPath(dbus_constants::kRootServicePath));
  on_buffet_command_callback_ = on_buffet_command_callback;
  object_manager_->RegisterInterface(dbus_constants::kCommandInterface, this);
  return true;
}

dbus::PropertySet* CommandListener::CreateProperties(
    dbus::ObjectProxy* object_proxy,
    const dbus::ObjectPath& object_path,
    const std::string& interface_name) {
  return new CommandPropertySet(object_proxy,
                                interface_name,
                                base::Bind(&CommandListener::OnPropertyChanged,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           object_path));
}

CommandPropertySet* CommandListener::GetProperties(
    const dbus::ObjectPath& object_path) const {
  auto props = object_manager_->GetProperties(
      object_path, dbus_constants::kCommandInterface);
  CHECK(props) << "Unable to get property set of D-Bus object at "
               << object_path.value();
  return static_cast<CommandPropertySet*>(props);
}

dbus::ObjectProxy* CommandListener::GetObjectProxy(
    const dbus::ObjectPath& object_path) const {
  auto proxy = object_manager_->GetObjectProxy(object_path);
  CHECK(proxy) << "Unable to get D-Bus object proxy for "
               << object_path.value();
  return proxy;
}

void CommandListener::ObjectAdded(const dbus::ObjectPath& object_path,
                                  const std::string& interface_name) {
  VLOG(1) << "D-Bus interface '" << interface_name
          << "' has been added for object at path '"
          << object_path.value() << "'.";
  scoped_ptr<Command> command(new Command(object_path, this));
  if (!on_buffet_command_callback_.is_null())
    on_buffet_command_callback_.Run(command.Pass());
}

void CommandListener::ObjectRemoved(const dbus::ObjectPath& object_path,
                                    const std::string& interface_name) {
  VLOG(1) << "D-Bus interface '" << interface_name
          << "' has been removed from object at path '"
          << object_path.value() << "'.";
}


void CommandListener::OnPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {
  VLOG(1) << "Value of property '" << property_name
          << "' on object at path '" << object_path.value() << "' has changed";
}

}  // namespace buffet

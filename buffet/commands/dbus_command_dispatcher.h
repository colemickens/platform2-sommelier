// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_DBUS_COMMAND_DISPATCHER_H_
#define BUFFET_COMMANDS_DBUS_COMMAND_DISPATCHER_H_

#include <map>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <dbus/bus.h>

#include "buffet/commands/command_dispatch_interface.h"
#include "buffet/commands/dbus_command_proxy.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos

namespace buffet {

// Implements D-Bus dispatch of commands. When OnCommandAdded is called over
// CommandDispachInterface, DBusCommandDispacher creates an instance of
// DBusCommandProxy object and advertises it through ExportedObjectManager on
// D-Bus. Command handling processes can watch the new D-Bus object appear
// and communicate with it to update the command handling progress.
// Once command is handled, DBusCommandProxy::Done() is called and the command
// is removed from the command queue and D-Bus ExportedObjectManager.
class DBusCommandDispacher : public CommandDispachInterface {
 public:
  DBusCommandDispacher(
      const scoped_refptr<dbus::Bus>& bus,
      chromeos::dbus_utils::ExportedObjectManager* object_manager = nullptr);
  virtual ~DBusCommandDispacher() = default;

  // CommandDispachInterface overrides. Called by CommandQueue.
  void OnCommandAdded(CommandInstance* command_instance) override;
  void OnCommandRemoved(CommandInstance* command_instance) override;

 protected:
  virtual std::unique_ptr<DBusCommandProxy> CreateDBusCommandProxy(
      CommandInstance* command_instance);

 private:
  scoped_refptr<dbus::Bus> bus_;
  base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager> object_manager_;
  int next_id_;
  // This is the map that tracks relationship between CommandInstance and
  // corresponding DBusCommandProxy objects.
  std::map<CommandInstance*, std::unique_ptr<DBusCommandProxy>> command_map_;

  // Default constructor is used in special circumstances such as for testing.
  DBusCommandDispacher() = default;

  friend class DBusCommandDispacherTest;
  friend class CommandManager;
  DISALLOW_COPY_AND_ASSIGN(DBusCommandDispacher);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_DBUS_COMMAND_DISPATCHER_H_

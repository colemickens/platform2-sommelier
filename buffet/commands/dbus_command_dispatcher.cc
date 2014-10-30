// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/dbus_command_dispatcher.h"

#include <chromeos/dbus/exported_object_manager.h>

#include "buffet/commands/command_instance.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

DBusCommandDispacher::DBusCommandDispacher(
    const scoped_refptr<dbus::Bus>& bus,
    ExportedObjectManager* object_manager)
    : bus_(bus), next_id_(0) {
  if (object_manager)
    object_manager_ = object_manager->AsWeakPtr();
}

void DBusCommandDispacher::OnCommandAdded(CommandInstance* command_instance) {
  auto proxy = CreateDBusCommandProxy(command_instance);
  proxy->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
  command_instance->AddProxy(proxy.get());

  auto pair = std::make_pair(command_instance, std::move(proxy));
  CHECK(command_map_.insert(std::move(pair)).second)
      << "The command instance is already in the dispatcher command map";
}

void DBusCommandDispacher::OnCommandRemoved(CommandInstance* command_instance) {
  command_instance->ClearProxies();
  CHECK_GT(command_map_.erase(command_instance), 0u)
      << "The command instance is not in the dispatcher command map";
}

DBusCommandProxy* DBusCommandDispacher::FindProxy(
    CommandInstance* command_instance) const {
  auto p = command_map_.find(command_instance);
  return p != command_map_.end() ? p->second.get() : nullptr;
}

std::unique_ptr<DBusCommandProxy> DBusCommandDispacher::CreateDBusCommandProxy(
      CommandInstance* command_instance) {
  return std::unique_ptr<DBusCommandProxy>(
      new DBusCommandProxy(object_manager_.get(),
                           bus_,
                           command_instance,
                           dbus_constants::kCommandServicePathPrefix +
                           std::to_string(++next_id_)));
}

}  // namespace buffet

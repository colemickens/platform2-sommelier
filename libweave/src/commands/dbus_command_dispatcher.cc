// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/dbus_command_dispatcher.h"

#include <chromeos/dbus/exported_object_manager.h>

#include "buffet/dbus_constants.h"
#include "libweave/src/commands/command_instance.h"
#include "libweave/src/commands/dbus_command_proxy.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace weave {

DBusCommandDispacher::DBusCommandDispacher(
    const base::WeakPtr<ExportedObjectManager>& object_manager)
    : object_manager_{object_manager} {
}

void DBusCommandDispacher::OnCommandAdded(CommandInstance* command_instance) {
  if (!object_manager_)
    return;
  std::unique_ptr<DBusCommandProxy> proxy{new DBusCommandProxy(
      object_manager_.get(), object_manager_->GetBus(), command_instance,
      buffet::kCommandServicePathPrefix + std::to_string(++next_id_))};
  proxy->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
  command_instance->AddObserver(proxy.release());
}

}  // namespace weave

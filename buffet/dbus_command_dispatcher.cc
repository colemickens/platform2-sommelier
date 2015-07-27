// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_command_dispatcher.h"

#include <chromeos/dbus/exported_object_manager.h>

#include "buffet/dbus_command_proxy.h"
#include "buffet/dbus_constants.h"
#include "weave/command.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

DBusCommandDispacher::DBusCommandDispacher(
    const base::WeakPtr<ExportedObjectManager>& object_manager,
    weave::Commands* command_manager)
    : object_manager_{object_manager} {
  command_manager->AddOnCommandAddedCallback(base::Bind(
      &DBusCommandDispacher::OnCommandAdded, weak_ptr_factory_.GetWeakPtr()));
}

void DBusCommandDispacher::OnCommandAdded(weave::Command* command) {
  if (!object_manager_)
    return;
  std::unique_ptr<DBusCommandProxy> proxy{new DBusCommandProxy(
      object_manager_.get(), object_manager_->GetBus(), command,
      buffet::kCommandServicePathPrefix + std::to_string(++next_id_))};
  proxy->RegisterAsync(AsyncEventSequencer::GetDefaultCompletionAction());
  command->AddObserver(proxy.release());
}

}  // namespace buffet

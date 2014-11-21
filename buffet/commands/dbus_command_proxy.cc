// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/dbus_command_proxy.h"

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "buffet/commands/command_instance.h"
#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_types.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

DBusCommandProxy::DBusCommandProxy(ExportedObjectManager* object_manager,
                                   const scoped_refptr<dbus::Bus>& bus,
                                   CommandInstance* command_instance,
                                   std::string object_path)
    : command_instance_{command_instance},
      dbus_object_{object_manager, bus, dbus::ObjectPath{object_path}} {
}

void DBusCommandProxy::RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback) {
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);

  // Set the initial property values before registering the DBus object.
  dbus_adaptor_.SetName(command_instance_->GetName());
  dbus_adaptor_.SetCategory(command_instance_->GetCategory());
  dbus_adaptor_.SetId(command_instance_->GetID());
  dbus_adaptor_.SetStatus(command_instance_->GetStatus());
  dbus_adaptor_.SetProgress(command_instance_->GetProgress());
  // Convert a string-to-PropValue map into a string-to-Any map which can be
  // sent over D-Bus.
  chromeos::VariantDictionary params;
  for (const auto& param_pair : command_instance_->GetParameters()) {
    params.insert(std::make_pair(param_pair.first,
                                 param_pair.second->GetValueAsAny()));
  }
  dbus_adaptor_.SetParameters(params);

  // Register the command DBus object and expose its methods and properties.
  dbus_object_.RegisterAsync(completion_callback);
}

void DBusCommandProxy::OnStatusChanged(const std::string& status) {
  dbus_adaptor_.SetStatus(status);
}

void DBusCommandProxy::OnProgressChanged(int progress) {
  dbus_adaptor_.SetProgress(progress);
}

bool DBusCommandProxy::SetProgress(chromeos::ErrorPtr* error,
                                   int32_t progress) {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::SetProgress("
            << progress << ")";

  // Validate |progress| parameter. Its value must be between 0 and 100.
  IntPropType progress_type;
  progress_type.AddMinMaxConstraint(0, 100);
  if (!progress_type.ValidateValue(progress, error))
    return false;

  command_instance_->SetProgress(progress);
  return true;
}

void DBusCommandProxy::Abort() {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Abort()";
  command_instance_->Abort();
}

void DBusCommandProxy::Cancel() {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Cancel()";
  command_instance_->Cancel();
}

void DBusCommandProxy::Done() {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Done()";
  command_instance_->Done();
}


}  // namespace buffet

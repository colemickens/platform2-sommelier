// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/dbus_command_proxy.h"

#include <chromeos/async_event_sequencer.h>
#include <chromeos/exported_object_manager.h>

#include "buffet/commands/command_instance.h"
#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_types.h"
#include "buffet/dbus_constants.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

DBusCommandProxy::DBusCommandProxy(ExportedObjectManager* object_manager,
                                   const scoped_refptr<dbus::Bus>& bus,
                                   CommandInstance* command_instance)
    : object_path_(dbus_constants::kCommandServicePathPrefix +
                   command_instance->GetID()),
      command_instance_(command_instance),
      dbus_object_(object_manager, bus, object_path_) {
}

void DBusCommandProxy::RegisterAsync(
      const AsyncEventSequencer::CompletionAction& completion_callback) {
  chromeos::dbus_utils::DBusInterface* itf =
    dbus_object_.AddOrGetInterface(dbus_constants::kCommandInterface);

  // DBus methods.
  itf->AddMethodHandler(dbus_constants::kCommandSetProgress,
                        base::Unretained(this),
                        &DBusCommandProxy::HandleSetProgress);
  itf->AddMethodHandler(dbus_constants::kCommandAbort,
                        base::Unretained(this),
                        &DBusCommandProxy::HandleAbort);
  itf->AddMethodHandler(dbus_constants::kCommandCancel,
                        base::Unretained(this),
                        &DBusCommandProxy::HandleCancel);
  itf->AddMethodHandler(dbus_constants::kCommandDone,
                        base::Unretained(this),
                        &DBusCommandProxy::HandleDone);

  // DBus properties.
  itf->AddProperty(dbus_constants::kCommandName, &name_);
  itf->AddProperty(dbus_constants::kCommandCategory, &category_);
  itf->AddProperty(dbus_constants::kCommandId, &id_);
  itf->AddProperty(dbus_constants::kCommandStatus, &status_);
  itf->AddProperty(dbus_constants::kCommandProgress, &progress_);

  // Set the initial property values before registering the DBus object.
  name_.SetValue(command_instance_->GetName());
  category_.SetValue(command_instance_->GetCategory());
  id_.SetValue(command_instance_->GetID());
  status_.SetValue(dbus_constants::kCommandStatusQueued);
  progress_.SetValue(0);

  // Register the command DBus object and expose its methods and properties.
  dbus_object_.RegisterAsync(completion_callback);
}

void DBusCommandProxy::HandleSetProgress(chromeos::ErrorPtr* error,
                                         int32_t progress) {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::SetProgress("
            << progress << ")";

  // Validate |progress| parameter. Its value must be between 0 and 100.
  IntPropType progress_type;
  progress_type.AddMinMaxConstraint(0, 100);
  if (progress_type.ValidateValue(progress, error)) {
    status_.SetValue(dbus_constants::kCommandStatusInProgress);
    progress_.SetValue(progress);
  }
}

void DBusCommandProxy::HandleAbort(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Abort()";
  status_.SetValue(dbus_constants::kCommandStatusAborted);
}

void DBusCommandProxy::HandleCancel(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Cancel()";
  status_.SetValue(dbus_constants::kCommandStatusCanceled);
}

void DBusCommandProxy::HandleDone(chromeos::ErrorPtr* error) {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Done()";
  status_.SetValue(dbus_constants::kCommandStatusDone);
  progress_.SetValue(100);
}


}  // namespace buffet

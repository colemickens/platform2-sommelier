// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/dbus_command_proxy.h"

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "buffet/commands/command_instance.h"
#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_types.h"
#include "buffet/libbuffet/dbus_constants.h"

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
  itf->AddSimpleMethodHandlerWithError(dbus_constants::kCommandSetProgress,
                                       base::Unretained(this),
                                       &DBusCommandProxy::HandleSetProgress);
  itf->AddSimpleMethodHandler(dbus_constants::kCommandAbort,
                              base::Unretained(this),
                              &DBusCommandProxy::HandleAbort);
  itf->AddSimpleMethodHandler(dbus_constants::kCommandCancel,
                              base::Unretained(this),
                              &DBusCommandProxy::HandleCancel);
  itf->AddSimpleMethodHandler(dbus_constants::kCommandDone,
                              base::Unretained(this),
                              &DBusCommandProxy::HandleDone);

  // DBus properties.
  itf->AddProperty(dbus_constants::kCommandName, &name_);
  itf->AddProperty(dbus_constants::kCommandCategory, &category_);
  itf->AddProperty(dbus_constants::kCommandId, &id_);
  itf->AddProperty(dbus_constants::kCommandStatus, &status_);
  itf->AddProperty(dbus_constants::kCommandProgress, &progress_);
  itf->AddProperty(dbus_constants::kCommandParameters, &parameters_);

  // Set the initial property values before registering the DBus object.
  name_.SetValue(command_instance_->GetName());
  category_.SetValue(command_instance_->GetCategory());
  id_.SetValue(command_instance_->GetID());
  status_.SetValue(command_instance_->GetStatus());
  progress_.SetValue(command_instance_->GetProgress());
  // Convert a string-to-PropValue map into a string-to-Any map which can be
  // sent over D-Bus.
  chromeos::VariantDictionary params;
  for (const auto& param_pair : command_instance_->GetParameters()) {
    params.insert(std::make_pair(param_pair.first,
                                 param_pair.second->GetValueAsAny()));
  }
  parameters_.SetValue(params);

  // Register the command DBus object and expose its methods and properties.
  dbus_object_.RegisterAsync(completion_callback);
}

void DBusCommandProxy::OnStatusChanged(const std::string& status) {
  status_.SetValue(status);
}

void DBusCommandProxy::OnProgressChanged(int progress) {
  progress_.SetValue(progress);
}

bool DBusCommandProxy::HandleSetProgress(chromeos::ErrorPtr* error,
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

void DBusCommandProxy::HandleAbort() {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Abort()";
  command_instance_->Abort();
}

void DBusCommandProxy::HandleCancel() {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Cancel()";
  command_instance_->Cancel();
}

void DBusCommandProxy::HandleDone() {
  LOG(INFO) << "Received call to Command<"
            << command_instance_->GetName() << ">::Done()";
  command_instance_->Done();
}


}  // namespace buffet

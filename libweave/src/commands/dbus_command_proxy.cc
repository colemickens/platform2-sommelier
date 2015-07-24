// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/dbus_command_proxy.h"

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>

#include "libweave/src/commands/dbus_conversion.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::ExportedObjectManager;

namespace weave {

DBusCommandProxy::DBusCommandProxy(ExportedObjectManager* object_manager,
                                   const scoped_refptr<dbus::Bus>& bus,
                                   Command* command,
                                   std::string object_path)
    : command_{command},
      dbus_object_{object_manager, bus, dbus::ObjectPath{object_path}} {}

void DBusCommandProxy::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);

  // Set the initial property values before registering the DBus object.
  dbus_adaptor_.SetName(command_->GetName());
  dbus_adaptor_.SetCategory(command_->GetCategory());
  dbus_adaptor_.SetId(command_->GetID());
  dbus_adaptor_.SetStatus(command_->GetStatus());
  dbus_adaptor_.SetProgress(
      DictionaryToDBusVariantDictionary(*command_->GetProgress()));
  dbus_adaptor_.SetOrigin(command_->GetOrigin());
  dbus_adaptor_.SetParameters(
      DictionaryToDBusVariantDictionary(*command_->GetParameters()));
  dbus_adaptor_.SetResults(
      DictionaryToDBusVariantDictionary(*command_->GetResults()));

  // Register the command DBus object and expose its methods and properties.
  dbus_object_.RegisterAsync(completion_callback);
}

void DBusCommandProxy::OnResultsChanged() {
  dbus_adaptor_.SetResults(
      DictionaryToDBusVariantDictionary(*command_->GetResults()));
}

void DBusCommandProxy::OnStatusChanged() {
  dbus_adaptor_.SetStatus(command_->GetStatus());
}

void DBusCommandProxy::OnProgressChanged() {
  dbus_adaptor_.SetProgress(
      DictionaryToDBusVariantDictionary(*command_->GetProgress()));
}

void DBusCommandProxy::OnCommandDestroyed() {
  delete this;
}

bool DBusCommandProxy::SetProgress(
    chromeos::ErrorPtr* error,
    const chromeos::VariantDictionary& progress) {
  LOG(INFO) << "Received call to Command<" << command_->GetName()
            << ">::SetProgress()";
  auto dictionary = DictionaryFromDBusVariantDictionary(progress, error);
  if (!dictionary)
    return false;
  return command_->SetProgress(*dictionary, error);
}

bool DBusCommandProxy::SetResults(chromeos::ErrorPtr* error,
                                  const chromeos::VariantDictionary& results) {
  LOG(INFO) << "Received call to Command<" << command_->GetName()
            << ">::SetResults()";
  auto dictionary = DictionaryFromDBusVariantDictionary(results, error);
  if (!dictionary)
    return false;
  return command_->SetResults(*dictionary, error);
}

void DBusCommandProxy::Abort() {
  LOG(INFO) << "Received call to Command<" << command_->GetName()
            << ">::Abort()";
  command_->Abort();
}

void DBusCommandProxy::Cancel() {
  LOG(INFO) << "Received call to Command<" << command_->GetName()
            << ">::Cancel()";
  command_->Cancel();
}

void DBusCommandProxy::Done() {
  LOG(INFO) << "Received call to Command<" << command_->GetName()
            << ">::Done()";
  command_->Done();
}

}  // namespace weave

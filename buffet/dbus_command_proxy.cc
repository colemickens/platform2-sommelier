// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_command_proxy.h"

#include <brillo/dbus/async_event_sequencer.h>
#include <brillo/dbus/exported_object_manager.h>
#include <weave/enum_to_string.h>

#include "buffet/dbus_conversion.h"
#include "buffet/weave_error_conversion.h"

using brillo::dbus_utils::AsyncEventSequencer;
using brillo::dbus_utils::ExportedObjectManager;

namespace errors {
namespace commands {
const char kDomain[] = "buffet";
const char kCommandDestroyed[] = "command_destroyed";
}  // namespace commands
}  // namespace errors

namespace buffet {

namespace {

bool ReportDestroyedError(brillo::ErrorPtr* error) {
  brillo::Error::AddTo(error, FROM_HERE, errors::commands::kDomain,
                       errors::commands::kCommandDestroyed,
                       "Command has been destroyed");
  return false;
}

}  // anonymous namespace

DBusCommandProxy::DBusCommandProxy(ExportedObjectManager* object_manager,
                                   const scoped_refptr<dbus::Bus>& bus,
                                   const std::weak_ptr<weave::Command>& command,
                                   std::string object_path)
    : command_{command},
      dbus_object_{object_manager, bus, dbus::ObjectPath{object_path}} {}

void DBusCommandProxy::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  auto command = command_.lock();
  if (!command)
    return;

  dbus_adaptor_.RegisterWithDBusObject(&dbus_object_);

  // Set the initial property values before registering the DBus object.
  dbus_adaptor_.SetName(command->GetName());
  dbus_adaptor_.SetId(command->GetID());
  dbus_adaptor_.SetState(EnumToString(command->GetState()));
  dbus_adaptor_.SetProgress(
      DictionaryToDBusVariantDictionary(*command->GetProgress()));
  dbus_adaptor_.SetOrigin(EnumToString(command->GetOrigin()));
  dbus_adaptor_.SetParameters(
      DictionaryToDBusVariantDictionary(*command->GetParameters()));
  dbus_adaptor_.SetResults(
      DictionaryToDBusVariantDictionary(*command->GetResults()));

  // Register the command DBus object and expose its methods and properties.
  dbus_object_.RegisterAsync(completion_callback);
}

bool DBusCommandProxy::SetProgress(brillo::ErrorPtr* error,
                                   const brillo::VariantDictionary& progress) {
  auto command = command_.lock();
  if (!command)
    return ReportDestroyedError(error);

  LOG(INFO) << "Received call to Command<" << command->GetName()
            << ">::SetProgress()";
  auto dictionary = DictionaryFromDBusVariantDictionary(progress, error);
  if (!dictionary)
    return false;
  weave::ErrorPtr weave_error;
  if (!command->SetProgress(*dictionary, &weave_error)) {
    ConvertError(*weave_error, error);
    return false;
  }
  dbus_adaptor_.SetProgress(
      DictionaryToDBusVariantDictionary(*command->GetProgress()));
  dbus_adaptor_.SetState(EnumToString(command->GetState()));
  return true;
}

bool DBusCommandProxy::Complete(brillo::ErrorPtr* error,
                                const brillo::VariantDictionary& results) {
  auto command = command_.lock();
  if (!command)
    return ReportDestroyedError(error);

  LOG(INFO) << "Received call to Command<" << command->GetName()
            << ">::Complete()";
  auto dictionary = DictionaryFromDBusVariantDictionary(results, error);
  if (!dictionary)
    return false;
  weave::ErrorPtr weave_error;
  if (!command->Complete(*dictionary, &weave_error)) {
    ConvertError(*weave_error, error);
    return false;
  }
  dbus_adaptor_.SetResults(
      DictionaryToDBusVariantDictionary(*command->GetResults()));
  dbus_adaptor_.SetState(EnumToString(command->GetState()));
  return true;
}

bool DBusCommandProxy::Abort(brillo::ErrorPtr* error,
                             const std::string& code,
                             const std::string& message) {
  auto command = command_.lock();
  if (!command)
    return ReportDestroyedError(error);

  LOG(INFO) << "Received call to Command<" << command->GetName()
            << ">::Abort()";
  weave::ErrorPtr cmd_error;
  if (!code.empty())
    weave::Error::AddTo(&cmd_error, FROM_HERE, "command_error", code, message);
  weave::ErrorPtr weave_error;
  if (!command->Abort(cmd_error.get(), &weave_error)) {
    ConvertError(*weave_error, error);
    return false;
  }
  dbus_adaptor_.SetState(EnumToString(command->GetState()));
  return true;
}

bool DBusCommandProxy::SetError(brillo::ErrorPtr* error,
                                const std::string& code,
                                const std::string& message) {
  auto command = command_.lock();
  if (!command)
    return ReportDestroyedError(error);

  LOG(INFO) << "Received call to Command<" << command->GetName()
            << ">::SetError()";
  weave::ErrorPtr cmd_error;
  if (!code.empty())
    weave::Error::AddTo(&cmd_error, FROM_HERE, "command_error", code, message);
  weave::ErrorPtr weave_error;
  if (!command->SetError(cmd_error.get(), &weave_error)) {
    ConvertError(*weave_error, error);
    return false;
  }
  dbus_adaptor_.SetState(EnumToString(command->GetState()));
  return true;
}

bool DBusCommandProxy::Cancel(brillo::ErrorPtr* error) {
  auto command = command_.lock();
  if (!command)
    return ReportDestroyedError(error);

  LOG(INFO) << "Received call to Command<" << command->GetName()
            << ">::Cancel()";
  weave::ErrorPtr weave_error;
  if (!command->Cancel(&weave_error)) {
    ConvertError(*weave_error, error);
    return false;
  }
  dbus_adaptor_.SetState(EnumToString(command->GetState()));
  return true;
}

}  // namespace buffet

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libbuffet/command.h"

#include <chromeos/dbus/dbus_method_invoker.h>

#include "libbuffet/command_listener.h"
#include "libbuffet/dbus_constants.h"
#include "libbuffet/private/command_property_set.h"

namespace buffet {

Command::Command(const dbus::ObjectPath& object_path,
                 CommandListener* command_listener)
    : object_path_(object_path), command_listener_(command_listener) {}

const std::string& Command::GetID() const {
  return GetProperties()->id.value();
}

const std::string& Command::GetName() const {
  return GetProperties()->name.value();
}

const std::string& Command::GetCategory() const {
  return GetProperties()->category.value();
}

const chromeos::dbus_utils::Dictionary& Command::GetParameters() const {
  return GetProperties()->parameters.value();
}

void Command::SetProgress(int progress) {
  chromeos::dbus_utils::CallMethodAndBlock(GetObjectProxy(),
                                           dbus_constants::kCommandInterface,
                                           dbus_constants::kCommandSetProgress,
                                           progress);
}

void Command::Abort() {
  chromeos::dbus_utils::CallMethodAndBlock(GetObjectProxy(),
                                           dbus_constants::kCommandInterface,
                                           dbus_constants::kCommandAbort);
}

void Command::Cancel() {
  chromeos::dbus_utils::CallMethodAndBlock(GetObjectProxy(),
                                           dbus_constants::kCommandInterface,
                                           dbus_constants::kCommandCancel);
}

void Command::Done() {
  chromeos::dbus_utils::CallMethodAndBlock(GetObjectProxy(),
                                           dbus_constants::kCommandInterface,
                                           dbus_constants::kCommandDone);
}

int Command::GetProgress() const {
  return GetProperties()->progress.value();
}

const std::string& Command::GetStatus() const {
  return GetProperties()->status.value();
}

CommandPropertySet* Command::GetProperties() const {
  return command_listener_->GetProperties(object_path_);
}

dbus::ObjectProxy* Command::GetObjectProxy() const {
  return command_listener_->GetObjectProxy(object_path_);
}

}  // namespace buffet

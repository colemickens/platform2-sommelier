// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_DBUS_COMMAND_PROXY_H_
#define BUFFET_COMMANDS_DBUS_COMMAND_PROXY_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/dbus/data_serialization.h>
#include <chromeos/dbus/dbus_object.h>

#include "buffet/commands/command_proxy_interface.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos

namespace buffet {

class CommandInstance;

class DBusCommandProxy : public CommandProxyInterface {
 public:
  DBusCommandProxy(chromeos::dbus_utils::ExportedObjectManager* object_manager,
                   const scoped_refptr<dbus::Bus>& bus,
                   CommandInstance* command_instance);
  ~DBusCommandProxy() override = default;

  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  // CommandProxyInterface implementation/overloads.
  void OnStatusChanged(const std::string& status) override;
  void OnProgressChanged(int progress) override;

 private:
  // DBus properties for org.chromium.Buffet.Command interface.
  chromeos::dbus_utils::ExportedProperty<std::string> name_;
  chromeos::dbus_utils::ExportedProperty<std::string> category_;
  chromeos::dbus_utils::ExportedProperty<std::string> id_;
  chromeos::dbus_utils::ExportedProperty<std::string> status_;
  chromeos::dbus_utils::ExportedProperty<int32_t> progress_;
  chromeos::dbus_utils::ExportedProperty<chromeos::VariantDictionary>
      parameters_;

  // Handles calls to org.chromium.Buffet.Command.SetProgress(progress).
  bool HandleSetProgress(chromeos::ErrorPtr* error, int32_t progress);
  // Handles calls to org.chromium.Buffet.Command.Abort().
  void HandleAbort();
  // Handles calls to org.chromium.Buffet.Command.Cancel().
  void HandleCancel();
  // Handles calls to org.chromium.Buffet.Command.Done().
  void HandleDone();

  dbus::ObjectPath object_path_;
  CommandInstance* command_instance_;

  chromeos::dbus_utils::DBusObject dbus_object_;

  friend class DBusCommandProxyTest;
  friend class DBusCommandDispacherTest;
  DISALLOW_COPY_AND_ASSIGN(DBusCommandProxy);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_DBUS_COMMAND_PROXY_H_

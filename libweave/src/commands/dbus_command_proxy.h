// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_DBUS_COMMAND_PROXY_H_
#define LIBWEAVE_SRC_COMMANDS_DBUS_COMMAND_PROXY_H_

#include <string>

#include <base/macros.h>
#include <chromeos/dbus/data_serialization.h>
#include <chromeos/dbus/dbus_object.h>

#include "buffet/org.chromium.Buffet.Command.h"
#include "libweave/src/commands/command_proxy_interface.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos

namespace weave {

class CommandInstance;

class DBusCommandProxy : public CommandObserver,
                         public org::chromium::Buffet::CommandInterface {
 public:
  DBusCommandProxy(chromeos::dbus_utils::ExportedObjectManager* object_manager,
                   const scoped_refptr<dbus::Bus>& bus,
                   CommandInstance* command_instance,
                   std::string object_path);
  ~DBusCommandProxy() override = default;

  void RegisterAsync(
      const chromeos::dbus_utils::AsyncEventSequencer::CompletionAction&
          completion_callback);

  // CommandProxyInterface implementation/overloads.
  void OnResultsChanged() override;
  void OnStatusChanged() override;
  void OnProgressChanged() override;
  void OnCommandDestroyed() override;

 private:
  // Handles calls to org.chromium.Buffet.Command.SetProgress(progress).
  bool SetProgress(chromeos::ErrorPtr* error,
                   const chromeos::VariantDictionary& progress) override;
  // Handles calls to org.chromium.Buffet.Command.SetResults(results).
  bool SetResults(chromeos::ErrorPtr* error,
                  const chromeos::VariantDictionary& results) override;
  // Handles calls to org.chromium.Buffet.Command.Abort().
  void Abort() override;
  // Handles calls to org.chromium.Buffet.Command.Cancel().
  void Cancel() override;
  // Handles calls to org.chromium.Buffet.Command.Done().
  void Done() override;

  CommandInstance* command_instance_;
  org::chromium::Buffet::CommandAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;

  friend class DBusCommandProxyTest;
  friend class DBusCommandDispacherTest;
  DISALLOW_COPY_AND_ASSIGN(DBusCommandProxy);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_DBUS_COMMAND_PROXY_H_

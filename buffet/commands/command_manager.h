// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_MANAGER_H_
#define BUFFET_COMMANDS_COMMAND_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/callback_list.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "buffet/commands/command_dictionary.h"
#include "buffet/commands/command_queue.h"
#include "buffet/commands/dbus_command_dispatcher.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos


namespace buffet {

class CommandInstance;

// CommandManager class that will have a list of all the device command
// schemas as well as the live command queue of pending command instances
// dispatched to the device.
class CommandManager final {
 public:
  // A token given by CommandManager in response to AddOnCommandDefChanged().
  // When the CallbackToken is destroyed, the registered notification
  // callback associated with it will automatically be removed from the command
  // manager's callback list.
  using CallbackToken =
      std::unique_ptr<base::CallbackList<void()>::Subscription>;

  CommandManager();
  explicit CommandManager(
      const base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager>&
          object_manager);
  // Special constructor to help mock out command dispatcher for testing.
  explicit CommandManager(CommandDispachInterface* dispatch_interface);

  // Sets callback which is called when command definitions is changed.
  CallbackToken AddOnCommandDefChanged(
      const base::Closure& callback) WARN_UNUSED_RESULT {
    return CallbackToken{on_command_changed_.Add(callback).release()};
  }

  // Returns the command definitions for the device.
  const CommandDictionary& GetCommandDictionary() const;

  // Loads base/standard GCD command definitions.
  // |json| is the full JSON schema of standard GCD commands. These commands
  // are not necessarily supported by a particular device but rather
  // all the standard commands defined by GCD standard for all known/supported
  // device kinds.
  // On success, returns true. Otherwise, |error| contains additional
  // error information.
  bool LoadBaseCommands(const base::DictionaryValue& json,
                        chromeos::ErrorPtr* error);

  // Same as the overload above, but takes a path to a json file to read
  // the base command definitions from.
  bool LoadBaseCommands(const base::FilePath& json_file_path,
                        chromeos::ErrorPtr* error);

  // Loads device command schema for particular category.
  // See CommandDictionary::LoadCommands for detailed description of the
  // parameters.
  bool LoadCommands(const base::DictionaryValue& json,
                    const std::string& category,
                    chromeos::ErrorPtr* error);

  // Same as the overload above, but takes a path to a json file to read
  // the base command definitions from. Also, the command category is
  // derived from file name (without extension). So, if the path points to
  // "power_manager.json", the command category used will be "power_manager".
  bool LoadCommands(const base::FilePath& json_file_path,
                    chromeos::ErrorPtr* error);

  // Startup method to be called by buffet daemon at startup.
  // Initializes the object and reads files in |definitions_path| to load
  //   1) the standard GCD command dictionary
  //   2) static vendor-provided command definitions
  // If |test_definitions_path| is not empty, we'll also look there for
  // additional commands.
  void Startup(const base::FilePath& definitions_path,
               const base::FilePath& test_definitions_path);

  // Adds a new command to the command queue.
  void AddCommand(std::unique_ptr<CommandInstance> command_instance);

  // Finds a command by the command |id|. Returns nullptr if the command with
  // the given |id| is not found. The returned pointer should not be persisted
  // for a long period of time.
  CommandInstance* FindCommand(const std::string& id) const;

  // Changes the visibility of commands.
  bool SetCommandVisibility(const std::vector<std::string>& command_names,
                            CommandDefinition::Visibility visibility,
                            chromeos::ErrorPtr* error);

 private:
  CommandDictionary base_dictionary_;  // Base/std command definitions/schemas.
  CommandDictionary dictionary_;  // Command definitions/schemas.
  CommandQueue command_queue_;
  DBusCommandDispacher command_dispatcher_;
  base::CallbackList<void()> on_command_changed_;

  DISALLOW_COPY_AND_ASSIGN(CommandManager);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_MANAGER_H_

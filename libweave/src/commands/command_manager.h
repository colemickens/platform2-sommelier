// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_COMMAND_MANAGER_H_
#define LIBWEAVE_SRC_COMMANDS_COMMAND_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "libweave/src/commands/command_dictionary.h"
#include "libweave/src/commands/command_queue.h"
#include "weave/commands.h"

namespace weave {

class CommandInstance;

// CommandManager class that will have a list of all the device command
// schemas as well as the live command queue of pending command instances
// dispatched to the device.
class CommandManager final : public Commands {
 public:
  CommandManager();

  ~CommandManager() override;

  // Commands overrides.
  bool AddCommand(const base::DictionaryValue& command,
                  UserRole role,
                  std::string* id,
                  chromeos::ErrorPtr* error) override;
  CommandInstance* FindCommand(const std::string& id) override;
  void AddOnCommandAddedCallback(const OnCommandCallback& callback) override;
  void AddOnCommandRemovedCallback(const OnCommandCallback& callback) override;

  // Sets callback which is called when command definitions is changed.
  void AddOnCommandDefChanged(const base::Closure& callback);

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

  // Changes the visibility of commands.
  bool SetCommandVisibility(const std::vector<std::string>& command_names,
                            CommandDefinition::Visibility visibility,
                            chromeos::ErrorPtr* error);

 private:
  CommandDictionary base_dictionary_;  // Base/std command definitions/schemas.
  CommandDictionary dictionary_;       // Command definitions/schemas.
  CommandQueue command_queue_;
  std::vector<base::Callback<void()>> on_command_changed_;
  uint32_t next_command_id_{0};

  DISALLOW_COPY_AND_ASSIGN(CommandManager);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_COMMAND_MANAGER_H_

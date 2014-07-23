// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_MANAGER_H_
#define BUFFET_COMMANDS_COMMAND_MANAGER_H_

#include <string>

#include <base/basictypes.h>
#include <base/files/file_path.h>

#include "buffet/commands/command_dictionary.h"

namespace buffet {

// CommandManager class that will have a list of all the device command
// schemas as well as the live command queue of pending command instances
// dispatched to the device.
class CommandManager {
 public:
  CommandManager() = default;

  // Get the command definitions for the device.
  const CommandDictionary& GetCommandDictionary() const;

  // Loads base/standard GCD command definitions.
  // |json| is the full JSON schema of standard GCD commands. These commands
  // are not necessarily supported by a particular device but rather
  // all the standard commands defined by GCD standard for all known/supported
  // device kinds.
  // On success, returns true. Otherwise, |error| contains additional
  // error information.
  bool LoadBaseCommands(const base::DictionaryValue& json,
                        ErrorPtr* error);

  // Same as the overload above, but takes a path to a json file to read
  // the base command definitions from.
  bool LoadBaseCommands(const base::FilePath& json_file_path,
                        ErrorPtr* error);

  // Loads device command schema for particular category.
  // See CommandDictionary::LoadCommands for detailed description of the
  // parameters.
  bool LoadCommands(const base::DictionaryValue& json,
                    const std::string& category, ErrorPtr* error);

  // Same as the overload above, but takes a path to a json file to read
  // the base command definitions from. Also, the command category is
  // derived from file name (without extension). So, if the path points to
  // "power_manager.json", the command category used will be "power_manager".
  bool LoadCommands(const base::FilePath& json_file_path,
                    ErrorPtr* error);

  // Startup method to be called by buffet daemon at startup.
  // Initializes the object and loads the standard GCD command
  // dictionary as well as static vendor-provided command definitions for
  // the current device.
  void Startup();

 private:
  // Helper function to load a JSON file that is expected to be
  // an object/dictionary. In case of error, returns empty unique ptr and fills
  // in error details in |error|.
  std::unique_ptr<const base::DictionaryValue> LoadJsonDict(
      const base::FilePath& json_file_path, ErrorPtr* error);

  CommandDictionary base_dictionary_;  // Base/std command definitions/schemas.
  CommandDictionary dictionary_;  // Command definitions/schemas.

  DISALLOW_COPY_AND_ASSIGN(CommandManager);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_MANAGER_H_

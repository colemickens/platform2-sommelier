// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_MANAGER_H_
#define BUFFET_COMMANDS_COMMAND_MANAGER_H_

#include <base/basictypes.h>

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

 private:
  CommandDictionary dictionary_;  // Command definitions/schemas.
  DISALLOW_COPY_AND_ASSIGN(CommandManager);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_MANAGER_H_

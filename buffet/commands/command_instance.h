// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_INSTANCE_H_
#define BUFFET_COMMANDS_COMMAND_INSTANCE_H_

#include <map>
#include <memory>
#include <string>

#include <base/basictypes.h>

#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_utils.h"
#include "buffet/error.h"

namespace base {
class Value;
}  // namespace base

namespace buffet {

class CommandDictionary;

class CommandInstance final {
 public:
  // Construct a command instance given the full command |name| which must
  // be in format "<package_name>.<command_name>", a command |category| and
  // a list of parameters and their values specified in |parameters|.
  CommandInstance(const std::string& name,
                  const std::string& category,
                  const native_types::Object& parameters);

  // Returns the full name of the command.
  const std::string& GetName() const { return name_; }
  // Returns the command category.
  const std::string& GetCategory() const { return category_; }
  // Returns the command parameters and their values.
  const native_types::Object& GetParameters() const { return parameters_; }
  // Finds a command parameter value by parameter |name|. If the parameter
  // with given name does not exist, returns null shared_ptr.
  std::shared_ptr<const PropValue> FindParameter(const std::string& name) const;

  // Parses a command instance JSON definition and constructs a CommandInstance
  // object, checking the JSON |value| against the command definition schema
  // found in command |dictionary|. On error, returns null unique_ptr and
  // fills in error details in |error|.
  static std::unique_ptr<const CommandInstance> FromJson(
      const base::Value* value,
      const CommandDictionary& dictionary,
      ErrorPtr* error);

 private:
  // Full command name as "<package_name>.<command_name>".
  std::string name_;
  // Command category. See comments for CommandDefinitions::LoadCommands for the
  // detailed description of what command categories are and what they are used
  // for.
  std::string category_;
  // Command parameters and their values.
  native_types::Object parameters_;

  DISALLOW_COPY_AND_ASSIGN(CommandInstance);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_INSTANCE_H_

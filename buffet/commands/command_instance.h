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

namespace buffet {

class CommandInstance final {
 public:
  // Construct a command instance given the full command |name| which must
  // be in format "<package_name>.<command_name>", a command |category| and
  // a list of parameters and their values specified in |parameters|.
  CommandInstance(const std::string& name, const std::string& category,
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

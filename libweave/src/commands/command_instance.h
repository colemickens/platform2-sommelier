// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_COMMAND_INSTANCE_H_
#define LIBWEAVE_SRC_COMMANDS_COMMAND_INSTANCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/errors/error.h>

#include "libweave/src/commands/prop_values.h"
#include "libweave/src/commands/schema_utils.h"
#include "weave/command.h"

namespace base {
class Value;
}  // namespace base

namespace weave {

class CommandDefinition;
class CommandDictionary;
class CommandObserver;
class CommandQueue;

class CommandInstance final : public Command {
 public:
  // Construct a command instance given the full command |name| which must
  // be in format "<package_name>.<command_name>", a command |category| and
  // a list of parameters and their values specified in |parameters|.
  CommandInstance(const std::string& name,
                  CommandOrigin origin,
                  const CommandDefinition* command_definition,
                  const ValueMap& parameters);
  ~CommandInstance() override;

  // Command overrides.
  std::unique_ptr<base::DictionaryValue> ToJson() const override;
  void AddObserver(Observer* observer) override;
  const std::string& GetID() const override;
  const std::string& GetName() const override;
  const std::string& GetCategory() const override;
  CommandStatus GetStatus() const override;
  CommandOrigin GetOrigin() const override;
  std::unique_ptr<base::DictionaryValue> GetParameters() const override;
  std::unique_ptr<base::DictionaryValue> GetProgress() const override;
  std::unique_ptr<base::DictionaryValue> GetResults() const override;
  bool SetProgress(const base::DictionaryValue& progress,
                   chromeos::ErrorPtr* error) override;
  bool SetResults(const base::DictionaryValue& results,
                  chromeos::ErrorPtr* error) override;
  void Abort() override;
  void Cancel() override;
  void Done() override;

  // Returns command definition.
  const CommandDefinition* GetCommandDefinition() const {
    return command_definition_;
  }

  // Parses a command instance JSON definition and constructs a CommandInstance
  // object, checking the JSON |value| against the command definition schema
  // found in command |dictionary|. On error, returns null unique_ptr and
  // fills in error details in |error|.
  // |command_id| is the ID of the command returned, as parsed from the |value|.
  // The command ID extracted (if present in the JSON object) even if other
  // parsing/validation error occurs and command instance is not constructed.
  // This is used to report parse failures back to the server.
  static std::unique_ptr<CommandInstance> FromJson(
      const base::Value* value,
      CommandOrigin origin,
      const CommandDictionary& dictionary,
      std::string* command_id,
      chromeos::ErrorPtr* error);

  // Sets the command ID (normally done by CommandQueue when the command
  // instance is added to it).
  void SetID(const std::string& id) { id_ = id; }
  // Sets the pointer to queue this command is part of.
  void SetCommandQueue(CommandQueue* queue) { queue_ = queue; }

 private:
  // Helper function to update the command status.
  // Used by Abort(), Cancel(), Done() methods.
  void SetStatus(CommandStatus status);
  // Helper method that removes this command from the command queue.
  // Note that since the command queue owns the lifetime of the command instance
  // object, removing a command from the queue will also destroy it.
  void RemoveFromQueue();

  // Unique command ID within a command queue.
  std::string id_;
  // Full command name as "<package_name>.<command_name>".
  std::string name_;
  // The origin of the command, either "local" or "cloud".
  CommandOrigin origin_ = CommandOrigin::kLocal;
  // Command definition.
  const CommandDefinition* command_definition_;
  // Command parameters and their values.
  ValueMap parameters_;
  // Current command execution progress.
  ValueMap progress_;
  // Command results.
  ValueMap results_;
  // Current command status.
  CommandStatus status_ = CommandStatus::kQueued;
  // Command observer for the command.
  std::vector<Observer*> observers_;
  // Pointer to the command queue this command instance is added to.
  // The queue owns the command instance, so it outlives this object.
  CommandQueue* queue_ = nullptr;

  friend class DBusCommandDispacherTest;
  friend class DBusCommandProxyTest;
  DISALLOW_COPY_AND_ASSIGN(CommandInstance);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_COMMAND_INSTANCE_H_

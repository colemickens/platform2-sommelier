// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_INSTANCE_H_
#define BUFFET_COMMANDS_COMMAND_INSTANCE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <chromeos/errors/error.h>

#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_utils.h"

namespace base {
class Value;
}  // namespace base

namespace buffet {

class CommandDefinition;
class CommandDictionary;
class CommandProxyInterface;
class CommandQueue;

class CommandInstance final {
 public:
  // Construct a command instance given the full command |name| which must
  // be in format "<package_name>.<command_name>", a command |category| and
  // a list of parameters and their values specified in |parameters|.
  CommandInstance(const std::string& name,
                  const std::string& origin,
                  const CommandDefinition* command_definition,
                  const native_types::Object& parameters);
  ~CommandInstance();

  // Returns the full command ID.
  const std::string& GetID() const { return id_; }
  // Returns the full name of the command.
  const std::string& GetName() const { return name_; }
  // Returns the command category.
  const std::string& GetCategory() const;
  // Returns the command parameters and their values.
  const native_types::Object& GetParameters() const { return parameters_; }
  // Returns the command results and their values.
  const native_types::Object& GetResults() const { return results_; }
  // Finds a command parameter value by parameter |name|. If the parameter
  // with given name does not exist, returns nullptr.
  const PropValue* FindParameter(const std::string& name) const;
  // Returns the full name of the command.
  const std::string& GetOrigin() const { return origin_; }

  // Returns command definition.
  const CommandDefinition* GetCommandDefinition() const {
    return command_definition_;
  }

  // Parses a command instance JSON definition and constructs a CommandInstance
  // object, checking the JSON |value| against the command definition schema
  // found in command |dictionary|. On error, returns null unique_ptr and
  // fills in error details in |error|.
  static std::unique_ptr<CommandInstance> FromJson(
      const base::Value* value,
      const std::string& origin,
      const CommandDictionary& dictionary,
      chromeos::ErrorPtr* error);

  // Returns JSON representation of the command.
  std::unique_ptr<base::DictionaryValue> ToJson() const;

  // Sets the command ID (normally done by CommandQueue when the command
  // instance is added to it).
  void SetID(const std::string& id) { id_ = id; }
  // Adds a proxy for this command.
  // The proxy object is not owned by this class.
  void AddProxy(std::unique_ptr<CommandProxyInterface> proxy);
  // Sets the pointer to queue this command is part of.
  void SetCommandQueue(CommandQueue* queue) { queue_ = queue; }

  // Updates the command progress. The |progress| should match the schema.
  // Returns false if |results| value is incorrect.
  bool SetProgress(const native_types::Object& progress);

  // Updates the command results. The |results| should match the schema.
  // Returns false if |results| value is incorrect.
  bool SetResults(const native_types::Object& results);

  // Aborts command execution.
  void Abort();
  // Cancels command execution.
  void Cancel();
  // Marks the command as completed successfully.
  void Done();

  // Command state getters.
  const native_types::Object& GetProgress() const { return progress_; }
  const std::string& GetStatus() const { return status_; }

  // Values for command execution status.
  static const char kStatusQueued[];
  static const char kStatusInProgress[];
  static const char kStatusPaused[];
  static const char kStatusError[];
  static const char kStatusDone[];
  static const char kStatusCancelled[];
  static const char kStatusAborted[];
  static const char kStatusExpired[];

 private:
  // Helper function to update the command status.
  // Used by Abort(), Cancel(), Done() methods.
  void SetStatus(const std::string& status);
  // Helper method that removes this command from the command queue.
  // Note that since the command queue owns the lifetime of the command instance
  // object, removing a command from the queue will also destroy it.
  void RemoveFromQueue();

  // Unique command ID within a command queue.
  std::string id_;
  // Full command name as "<package_name>.<command_name>".
  std::string name_;
  // The origin of the command, either "local" or "cloud".
  std::string origin_;
  // Command definition.
  const CommandDefinition* command_definition_;
  // Command parameters and their values.
  native_types::Object parameters_;
  // Current command execution progress.
  native_types::Object progress_;
  // Command results.
  native_types::Object results_;
  // Current command status.
  std::string status_ = kStatusQueued;
  // Command proxies for the command.
  std::vector<std::unique_ptr<CommandProxyInterface>> proxies_;
  // Pointer to the command queue this command instance is added to.
  // The queue owns the command instance, so it outlives this object.
  CommandQueue* queue_ = nullptr;

  friend class DBusCommandDispacherTest;
  friend class DBusCommandProxyTest;
  DISALLOW_COPY_AND_ASSIGN(CommandInstance);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_INSTANCE_H_

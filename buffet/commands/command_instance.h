// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_COMMANDS_COMMAND_INSTANCE_H_
#define BUFFET_COMMANDS_COMMAND_INSTANCE_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>
#include <chromeos/errors/error.h>

#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_utils.h"

namespace base {
class Value;
}  // namespace base

namespace buffet {

class CommandDictionary;
class CommandProxyInterface;
class CommandQueue;

class CommandInstance final {
 public:
  // Construct a command instance given the full command |name| which must
  // be in format "<package_name>.<command_name>", a command |category| and
  // a list of parameters and their values specified in |parameters|.
  CommandInstance(const std::string& name,
                  const std::string& category,
                  const native_types::Object& parameters);

  // Returns the full command ID.
  const std::string& GetID() const { return id_; }
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
  static std::unique_ptr<CommandInstance> FromJson(
      const base::Value* value,
      const CommandDictionary& dictionary,
      chromeos::ErrorPtr* error);

  // Sets the command ID (normally done by CommandQueue when the command
  // instance is added to it).
  void SetID(const std::string& id) { id_ = id; }
  // Sets the command proxy for the dispatch technology used.
  // The proxy object is not owned by this class.
  void SetProxy(CommandProxyInterface* proxy) { proxy_ = proxy; }
  // Sets the pointer to queue this command is part of.
  void SetCommandQueue(CommandQueue* queue) { queue_ = queue; }

  // Updates the command execution progress. The |progress| must be between
  // 0 and 100. Returns false if the progress value is incorrect.
  bool SetProgress(int progress);
  // Aborts command execution.
  void Abort();
  // Cancels command execution.
  void Cancel();
  // Marks the command as completed successfully.
  void Done();

  // Command state getters.
  int GetProgress() const { return progress_; }
  const std::string& GetStatus() const { return status_; }

  // Values for command execution status.
  static const char kStatusQueued[];
  static const char kStatusInProgress[];
  static const char kStatusPaused[];
  static const char kStatusError[];
  static const char kStatusDone[];
  static const char kStatusCanceled[];
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
  // Command category. See comments for CommandDefinitions::LoadCommands for the
  // detailed description of what command categories are and what they are used
  // for.
  std::string category_;
  // Command parameters and their values.
  native_types::Object parameters_;
  // Current command status.
  std::string status_ = kStatusQueued;
  // Current command execution progress.
  int progress_ = 0;
  // Command proxy class for the current dispatch technology (e.g. D-Bus).
  // This is a weak pointer. The proxy's lifetime is managed by the command
  // dispatcher.
  CommandProxyInterface* proxy_ = nullptr;
  // Pointer to the command queue this command instance is added to.
  // The queue owns the command instance, so it outlives this object.
  CommandQueue* queue_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CommandInstance);
};

}  // namespace buffet

#endif  // BUFFET_COMMANDS_COMMAND_INSTANCE_H_

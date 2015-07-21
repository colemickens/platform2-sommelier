// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/command_instance.h"

#include <base/values.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>

#include "libweave/src/commands/command_definition.h"
#include "libweave/src/commands/command_dictionary.h"
#include "libweave/src/commands/command_proxy_interface.h"
#include "libweave/src/commands/command_queue.h"
#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/schema_constants.h"
#include "libweave/src/commands/schema_utils.h"

namespace weave {

const char CommandInstance::kStatusQueued[] = "queued";
const char CommandInstance::kStatusInProgress[] = "inProgress";
const char CommandInstance::kStatusPaused[] = "paused";
const char CommandInstance::kStatusError[] = "error";
const char CommandInstance::kStatusDone[] = "done";
const char CommandInstance::kStatusCancelled[] = "cancelled";
const char CommandInstance::kStatusAborted[] = "aborted";
const char CommandInstance::kStatusExpired[] = "expired";

CommandInstance::CommandInstance(const std::string& name,
                                 const std::string& origin,
                                 const CommandDefinition* command_definition,
                                 const native_types::Object& parameters)
    : name_{name},
      origin_{origin},
      command_definition_{command_definition},
      parameters_{parameters} {
  CHECK(command_definition_);
}

CommandInstance::~CommandInstance() {
  for (auto& proxy : proxies_) {
    proxy->OnCommandDestroyed();
  }
}

const std::string& CommandInstance::GetCategory() const {
  return command_definition_->GetCategory();
}

const PropValue* CommandInstance::FindParameter(const std::string& name) const {
  auto p = parameters_.find(name);
  return (p != parameters_.end()) ? p->second.get() : nullptr;
}

namespace {

// Helper method to retrieve command parameters from the command definition
// object passed in as |json| and corresponding command definition schema
// specified in |command_def|.
// On success, returns |true| and the validated parameters and values through
// |parameters|. Otherwise returns |false| and additional error information in
// |error|.
bool GetCommandParameters(const base::DictionaryValue* json,
                          const CommandDefinition* command_def,
                          native_types::Object* parameters,
                          chromeos::ErrorPtr* error) {
  // Get the command parameters from 'parameters' property.
  base::DictionaryValue no_params;  // Placeholder when no params are specified.
  const base::DictionaryValue* params = nullptr;
  const base::Value* params_value = nullptr;
  if (json->Get(commands::attributes::kCommand_Parameters, &params_value)) {
    // Make sure the "parameters" property is actually an object.
    if (!params_value->GetAsDictionary(&params)) {
      chromeos::Error::AddToPrintf(error, FROM_HERE,
                                   chromeos::errors::json::kDomain,
                                   chromeos::errors::json::kObjectExpected,
                                   "Property '%s' must be a JSON object",
                                   commands::attributes::kCommand_Parameters);
      return false;
    }
  } else {
    // "parameters" are not specified. Assume empty param list.
    params = &no_params;
  }

  // Now read in the parameters and validate their values against the command
  // definition schema.
  ObjectPropType obj_prop_type;
  obj_prop_type.SetObjectSchema(command_def->GetParameters()->Clone());
  if (!TypedValueFromJson(params, &obj_prop_type, parameters, error)) {
    return false;
  }
  return true;
}

}  // anonymous namespace

std::unique_ptr<CommandInstance> CommandInstance::FromJson(
    const base::Value* value,
    const std::string& origin,
    const CommandDictionary& dictionary,
    std::string* command_id,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<CommandInstance> instance;
  std::string command_id_buffer;  // used if |command_id| was nullptr.
  if (!command_id)
    command_id = &command_id_buffer;

  // Get the command JSON object from the value.
  const base::DictionaryValue* json = nullptr;
  if (!value->GetAsDictionary(&json)) {
    chromeos::Error::AddTo(error, FROM_HERE, chromeos::errors::json::kDomain,
                           chromeos::errors::json::kObjectExpected,
                           "Command instance is not a JSON object");
    command_id->clear();
    return instance;
  }

  // Get the command ID from 'id' property.
  if (!json->GetString(commands::attributes::kCommand_Id, command_id))
    command_id->clear();

  // Get the command name from 'name' property.
  std::string command_name;
  if (!json->GetString(commands::attributes::kCommand_Name, &command_name)) {
    chromeos::Error::AddTo(error, FROM_HERE, errors::commands::kDomain,
                           errors::commands::kPropertyMissing,
                           "Command name is missing");
    return instance;
  }
  // Make sure we know how to handle the command with this name.
  auto command_def = dictionary.FindCommand(command_name);
  if (!command_def) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kInvalidCommandName,
                                 "Unknown command received: %s",
                                 command_name.c_str());
    return instance;
  }

  native_types::Object parameters;
  if (!GetCommandParameters(json, command_def, &parameters, error)) {
    chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                 errors::commands::kCommandFailed,
                                 "Failed to validate command '%s'",
                                 command_name.c_str());
    return instance;
  }

  instance.reset(
      new CommandInstance{command_name, origin, command_def, parameters});

  if (!command_id->empty())
    instance->SetID(*command_id);

  return instance;
}

std::unique_ptr<base::DictionaryValue> CommandInstance::ToJson() const {
  std::unique_ptr<base::DictionaryValue> json{new base::DictionaryValue};

  json->SetString(commands::attributes::kCommand_Id, id_);
  json->SetString(commands::attributes::kCommand_Name, name_);
  json->Set(commands::attributes::kCommand_Parameters,
            TypedValueToJson(parameters_, nullptr).release());
  json->Set(commands::attributes::kCommand_Progress,
            TypedValueToJson(progress_, nullptr).release());
  json->Set(commands::attributes::kCommand_Results,
            TypedValueToJson(results_, nullptr).release());
  json->SetString(commands::attributes::kCommand_State, status_);

  return json;
}

void CommandInstance::AddProxy(CommandProxyInterface* proxy) {
  proxies_.push_back(proxy);
}

bool CommandInstance::SetResults(const native_types::Object& results) {
  // TODO(antonm): Add validation.
  if (results != results_) {
    results_ = results;
    for (auto& proxy : proxies_) {
      proxy->OnResultsChanged();
    }
  }
  return true;
}

bool CommandInstance::SetProgress(const native_types::Object& progress) {
  // Change status even if progress unchanged, e.g. 0% -> 0%.
  SetStatus(kStatusInProgress);
  if (progress != progress_) {
    progress_ = progress;
    for (auto& proxy : proxies_) {
      proxy->OnProgressChanged();
    }
  }
  return true;
}

void CommandInstance::Abort() {
  SetStatus(kStatusAborted);
  RemoveFromQueue();
  // The command will be destroyed after that, so do not access any members.
}

void CommandInstance::Cancel() {
  SetStatus(kStatusCancelled);
  RemoveFromQueue();
  // The command will be destroyed after that, so do not access any members.
}

void CommandInstance::Done() {
  SetStatus(kStatusDone);
  RemoveFromQueue();
  // The command will be destroyed after that, so do not access any members.
}

void CommandInstance::SetStatus(const std::string& status) {
  if (status != status_) {
    status_ = status;
    for (auto& proxy : proxies_) {
      proxy->OnStatusChanged();
    }
  }
}

void CommandInstance::RemoveFromQueue() {
  if (queue_)
    queue_->DelayedRemove(GetID());
}

}  // namespace weave

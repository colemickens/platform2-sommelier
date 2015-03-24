// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_instance.h"

#include <base/values.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>

#include "buffet/commands/command_definition.h"
#include "buffet/commands/command_dictionary.h"
#include "buffet/commands/command_proxy_interface.h"
#include "buffet/commands/command_queue.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/commands/schema_utils.h"

namespace buffet {

const char CommandInstance::kStatusQueued[] = "queued";
const char CommandInstance::kStatusInProgress[] = "inProgress";
const char CommandInstance::kStatusPaused[] = "paused";
const char CommandInstance::kStatusError[] = "error";
const char CommandInstance::kStatusDone[] = "done";
const char CommandInstance::kStatusCancelled[] = "cancelled";
const char CommandInstance::kStatusAborted[] = "aborted";
const char CommandInstance::kStatusExpired[] = "expired";

CommandInstance::CommandInstance(const std::string& name,
                                 const CommandDefinition* command_definition,
                                 const native_types::Object& parameters)
    : name_{name},
      command_definition_{command_definition},
      parameters_{parameters} {
  CHECK(command_definition_);
}

CommandInstance::~CommandInstance() = default;

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
  if (json->GetWithoutPathExpansion(commands::attributes::kCommand_Parameters,
                                    &params_value)) {
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
    const CommandDictionary& dictionary,
    chromeos::ErrorPtr* error) {
  std::unique_ptr<CommandInstance> instance;
  // Get the command JSON object from the value.
  const base::DictionaryValue* json = nullptr;
  if (!value->GetAsDictionary(&json)) {
    chromeos::Error::AddTo(error, FROM_HERE, chromeos::errors::json::kDomain,
                           chromeos::errors::json::kObjectExpected,
                           "Command instance is not a JSON object");
    return instance;
  }

  // Get the command name from 'name' property.
  std::string command_name;
  if (!json->GetStringWithoutPathExpansion(commands::attributes::kCommand_Name,
                                           &command_name)) {
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

  instance.reset(new CommandInstance(command_name, command_def, parameters));
  std::string command_id;
  if (json->GetStringWithoutPathExpansion(commands::attributes::kCommand_Id,
                                          &command_id)) {
    instance->SetID(command_id);
  }

  return instance;
}

std::unique_ptr<base::DictionaryValue> CommandInstance::ToJson() const {
  std::unique_ptr<base::DictionaryValue> json{new base::DictionaryValue};

  json->SetString(commands::attributes::kCommand_Id, id_);
  json->SetString(commands::attributes::kCommand_Name, name_);
  json->Set(commands::attributes::kCommand_Parameters,
            TypedValueToJson(parameters_, nullptr).release());
  json->Set(commands::attributes::kCommand_Results,
            TypedValueToJson(results_, nullptr).release());
  json->SetInteger(commands::attributes::kCommand_Progress, progress_);
  json->SetString(commands::attributes::kCommand_State, status_);

  return json;
}

void CommandInstance::AddProxy(std::unique_ptr<CommandProxyInterface> proxy) {
  proxies_.push_back(std::move(proxy));
}

bool CommandInstance::SetResults(const native_types::Object& results) {
  // TODO(antonm): Add validation.
  if (results != results_) {
    results_ = results;
    for (auto& proxy : proxies_) {
      proxy->OnResultsChanged(results_);
    }
  }
  return true;
}

bool CommandInstance::SetProgress(int progress) {
  if (progress < 0 || progress > 100)
    return false;
  if (progress != progress_) {
    progress_ = progress;
    SetStatus(kStatusInProgress);
    for (auto& proxy : proxies_) {
      proxy->OnProgressChanged(progress_);
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
  SetProgress(100);
  SetStatus(kStatusDone);
  RemoveFromQueue();
  // The command will be destroyed after that, so do not access any members.
}

void CommandInstance::SetStatus(const std::string& status) {
  if (status != status_) {
    status_ = status;
    for (auto& proxy : proxies_) {
      proxy->OnStatusChanged(status_);
    }
  }
}

void CommandInstance::RemoveFromQueue() {
  if (queue_) {
    // Store this instance in unique_ptr until the end of this method,
    // otherwise it will be destroyed as soon as CommandQueue::Remove() returns.
    std::unique_ptr<CommandInstance> this_instance = queue_->Remove(GetID());
    // The command instance will survive till the end of this scope.
  }
}


}  // namespace buffet

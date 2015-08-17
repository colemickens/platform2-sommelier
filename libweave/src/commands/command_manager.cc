// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/command_manager.h"

#include <base/files/file_enumerator.h>
#include <base/values.h>
#include <weave/config_store.h>
#include <weave/enum_to_string.h>
#include <weave/error.h>

#include "libweave/src/commands/schema_constants.h"
#include "libweave/src/utils.h"

namespace weave {

CommandManager::CommandManager() {}

CommandManager::~CommandManager() {}

void CommandManager::AddOnCommandDefChanged(const base::Closure& callback) {
  on_command_changed_.push_back(callback);
  callback.Run();
}

const CommandDictionary& CommandManager::GetCommandDictionary() const {
  return dictionary_;
}

bool CommandManager::LoadBaseCommands(const base::DictionaryValue& dict,
                                      ErrorPtr* error) {
  return base_dictionary_.LoadCommands(dict, "", nullptr, error);
}

bool CommandManager::LoadBaseCommands(const std::string& json,
                                      ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> dict = LoadJsonDict(json, error);
  if (!dict)
    return false;
  return LoadBaseCommands(*dict, error);
}

bool CommandManager::LoadCommands(const base::DictionaryValue& dict,
                                  const std::string& category,
                                  ErrorPtr* error) {
  bool result =
      dictionary_.LoadCommands(dict, category, &base_dictionary_, error);
  for (const auto& cb : on_command_changed_)
    cb.Run();
  return result;
}

bool CommandManager::LoadCommands(const std::string& json,
                                  const std::string& category,
                                  ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> dict = LoadJsonDict(json, error);
  if (!dict)
    return false;
  return LoadCommands(*dict, category, error);
}

void CommandManager::Startup(ConfigStore* config_store) {
  LOG(INFO) << "Initializing CommandManager.";

  // Load global standard GCD command dictionary.
  CHECK(LoadBaseCommands(config_store->LoadBaseCommandDefs(), nullptr));

  // Loading the rest of commands.
  for (const auto& defs : config_store->LoadCommandDefs())
    CHECK(this->LoadCommands(defs.second, defs.first, nullptr));
}

void CommandManager::AddCommand(
    std::unique_ptr<CommandInstance> command_instance) {
  command_queue_.Add(std::move(command_instance));
}

bool CommandManager::AddCommand(const base::DictionaryValue& command,
                                UserRole role,
                                std::string* id,
                                ErrorPtr* error) {
  auto command_instance = CommandInstance::FromJson(
      &command, CommandOrigin::kLocal, GetCommandDictionary(), nullptr, error);
  if (!command_instance)
    return false;

  UserRole minimal_role =
      command_instance->GetCommandDefinition()->GetMinimalRole();
  if (role < minimal_role) {
    Error::AddToPrintf(
        error, FROM_HERE, errors::commands::kDomain, "access_denied",
        "User role '%s' less than minimal: '%s'", EnumToString(role).c_str(),
        EnumToString(minimal_role).c_str());
    return false;
  }

  *id = std::to_string(++next_command_id_);
  command_instance->SetID(*id);
  AddCommand(std::move(command_instance));
  return true;
}

CommandInstance* CommandManager::FindCommand(const std::string& id) {
  return command_queue_.Find(id);
}

bool CommandManager::SetCommandVisibility(
    const std::vector<std::string>& command_names,
    CommandDefinition::Visibility visibility,
    ErrorPtr* error) {
  if (command_names.empty())
    return true;

  std::vector<CommandDefinition*> definitions;
  definitions.reserve(command_names.size());

  // Find/validate command definitions first.
  for (const std::string& name : command_names) {
    CommandDefinition* def = dictionary_.FindCommand(name);
    if (!def) {
      Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                         errors::commands::kInvalidCommandName,
                         "Command '%s' is unknown", name.c_str());
      return false;
    }
    definitions.push_back(def);
  }

  // Now that we know that all the command names were valid,
  // update the respective commands' visibility.
  for (CommandDefinition* def : definitions)
    def->SetVisibility(visibility);
  for (const auto& cb : on_command_changed_)
    cb.Run();
  return true;
}

void CommandManager::AddOnCommandAddedCallback(
    const OnCommandCallback& callback) {
  command_queue_.AddOnCommandAddedCallback(callback);
}

void CommandManager::AddOnCommandRemovedCallback(
    const OnCommandCallback& callback) {
  command_queue_.AddOnCommandRemovedCallback(callback);
}

}  // namespace weave

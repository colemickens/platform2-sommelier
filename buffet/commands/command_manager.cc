// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_manager.h"

#include <base/files/file_enumerator.h>
#include <base/values.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>

#include "buffet/commands/schema_constants.h"
#include "buffet/utils.h"

using chromeos::dbus_utils::ExportedObjectManager;

namespace buffet {

CommandManager::CommandManager() {
  command_queue_.SetCommandDispachInterface(&command_dispatcher_);
}

CommandManager::CommandManager(
    const base::WeakPtr<ExportedObjectManager>& object_manager)
    : command_dispatcher_(object_manager->GetBus(), object_manager.get()) {
  command_queue_.SetCommandDispachInterface(&command_dispatcher_);
}

const CommandDictionary& CommandManager::GetCommandDictionary() const {
  return dictionary_;
}

bool CommandManager::LoadBaseCommands(const base::DictionaryValue& json,
                                      chromeos::ErrorPtr* error) {
  return base_dictionary_.LoadCommands(json, "", nullptr, error);
}

bool CommandManager::LoadBaseCommands(const base::FilePath& json_file_path,
                                      chromeos::ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json = LoadJsonDict(
      json_file_path, error);
  if (!json)
    return false;
  return LoadBaseCommands(*json, error);
}

bool CommandManager::LoadCommands(const base::DictionaryValue& json,
                                  const std::string& category,
                                  chromeos::ErrorPtr* error) {
  return dictionary_.LoadCommands(json, category, &base_dictionary_, error);
}

bool CommandManager::LoadCommands(const base::FilePath& json_file_path,
                                  chromeos::ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json = LoadJsonDict(
      json_file_path, error);
  if (!json)
    return false;
  std::string category = json_file_path.BaseName().RemoveExtension().value();
  return LoadCommands(*json, category, error);
}

void CommandManager::Startup() {
  LOG(INFO) << "Initializing CommandManager.";
  // Load global standard GCD command dictionary.
  base::FilePath base_command_file("/etc/buffet/gcd.json");
  LOG(INFO) << "Loading standard commands from " << base_command_file.value();
  CHECK(LoadBaseCommands(base_command_file, nullptr))
      << "Failed to load the standard command definitions.";

  // Load static device command definitions.
  base::FilePath device_command_dir("/etc/buffet/commands");
  base::FileEnumerator enumerator(device_command_dir, false,
                                  base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.json"));
  base::FilePath json_file_path = enumerator.Next();
  while (!json_file_path.empty()) {
    LOG(INFO) << "Loading command schema from " << json_file_path.value();
    CHECK(LoadCommands(json_file_path, nullptr))
        << "Failed to load the command definition file.";
    json_file_path = enumerator.Next();
  }
}

void CommandManager::AddCommand(
    std::unique_ptr<CommandInstance> command_instance) {
  command_queue_.Add(std::move(command_instance));
}

CommandInstance* CommandManager::FindCommand(const std::string& id) const {
  return command_queue_.Find(id);
}

}  // namespace buffet

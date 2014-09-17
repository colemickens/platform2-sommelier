// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_manager.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/json/json_reader.h>
#include <base/values.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>

#include "buffet/commands/schema_constants.h"

using chromeos::dbus_utils::ExportedObjectManager;

namespace {

const char kCommandManagerErrorDomain[] = "Buffet_CommandManager";
const char kCommandManagerFileReadError[] = "file_read_error";

}  // namespace

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

std::unique_ptr<const base::DictionaryValue> CommandManager::LoadJsonDict(
    const base::FilePath& json_file_path, chromeos::ErrorPtr* error) {
  std::string json_string;
  if (!base::ReadFileToString(json_file_path, &json_string)) {
    chromeos::errors::system::AddSystemError(error, errno);
    chromeos::Error::AddToPrintf(error, kCommandManagerErrorDomain,
                                 kCommandManagerFileReadError,
                                 "Failed to read file '%s'",
                                 json_file_path.value().c_str());
    return std::unique_ptr<const base::DictionaryValue>();
  }
  std::string error_message;
  base::Value* value = base::JSONReader::ReadAndReturnError(
      json_string, base::JSON_PARSE_RFC, nullptr, &error_message);
  if (!value) {
    chromeos::Error::AddToPrintf(error, chromeos::errors::json::kDomain,
                                 chromeos::errors::json::kParseError,
                                 "Error parsing content of JSON file '%s': %s",
                                 json_file_path.value().c_str(),
                                 error_message.c_str());
    return std::unique_ptr<const base::DictionaryValue>();
  }
  const base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value)) {
    delete value;
    chromeos::Error::AddToPrintf(error, chromeos::errors::json::kDomain,
                                 chromeos::errors::json::kObjectExpected,
                                 "Content of file '%s' is not a JSON object",
                                 json_file_path.value().c_str());
    return std::unique_ptr<const base::DictionaryValue>();
  }
  return std::unique_ptr<const base::DictionaryValue>(dict_value);
}

std::string CommandManager::AddCommand(
    std::unique_ptr<CommandInstance> command_instance) {
  return command_queue_.Add(std::move(command_instance));
}

}  // namespace buffet

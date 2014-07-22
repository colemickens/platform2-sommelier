// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_manager.h"

#include <base/file_util.h>
#include <base/values.h>
#include <base/json/json_reader.h>

#include "buffet/commands/schema_constants.h"
#include "buffet/error_codes.h"

namespace buffet {

const CommandDictionary& CommandManager::GetCommandDictionary() const {
  return dictionary_;
}

bool CommandManager::LoadBaseCommands(const base::DictionaryValue& json,
                                      ErrorPtr* error) {
  return base_dictionary_.LoadCommands(json, "", nullptr, error);
}

bool CommandManager::LoadBaseCommands(const base::FilePath& json_file_path,
                                      ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json = LoadJsonDict(
      json_file_path, error);
  if (!json)
    return false;
  return LoadBaseCommands(*json, error);
}

bool CommandManager::LoadCommands(const base::DictionaryValue& json,
                                  const std::string& category,
                                  ErrorPtr* error) {
  return dictionary_.LoadCommands(json, category, &base_dictionary_, error);
}

bool CommandManager::LoadCommands(const base::FilePath& json_file_path,
                                  ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json = LoadJsonDict(
      json_file_path, error);
  if (!json)
    return false;
  std::string category = json_file_path.BaseName().RemoveExtension().value();
  return LoadCommands(*json, category, error);
}

std::unique_ptr<const base::DictionaryValue> CommandManager::LoadJsonDict(
    const base::FilePath& json_file_path, ErrorPtr* error) {
  std::string json_string;
  if (!base::ReadFileToString(json_file_path, &json_string)) {
    Error::AddToPrintf(error, errors::file_system::kDomain,
                       errors::file_system::kFileReadError,
                       "Failed to read file '%s'",
                       json_file_path.value().c_str());
    return std::unique_ptr<const base::DictionaryValue>();
  }
  std::string error_message;
  base::Value* value = base::JSONReader::ReadAndReturnError(
      json_string, base::JSON_PARSE_RFC, nullptr, &error_message);
  if (!value) {
    Error::AddToPrintf(error, errors::json::kDomain, errors::json::kParseError,
                       "Error parsing content of JSON file '%s': %s",
                       json_file_path.value().c_str(), error_message.c_str());
    return std::unique_ptr<const base::DictionaryValue>();
  }
  const base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value)) {
    delete value;
    Error::AddToPrintf(error, errors::json::kDomain,
                       errors::json::kObjectExpected,
                       "Content of file '%s' is not a JSON object",
                       json_file_path.value().c_str());
    return std::unique_ptr<const base::DictionaryValue>();
  }
  return std::unique_ptr<const base::DictionaryValue>(dict_value);
}

}  // namespace buffet

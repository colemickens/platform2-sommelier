// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_dictionary.h"

#include <base/values.h>
#include <chromeos/strings/string_utils.h>

#include "buffet/commands/command_definition.h"
#include "buffet/commands/schema_constants.h"

namespace buffet {

std::vector<std::string> CommandDictionary::GetCommandNamesByCategory(
    const std::string& category) const {
  std::vector<std::string> names;
  for (const auto& pair : definitions_) {
    if (pair.second->GetCategory() == category)
      names.push_back(pair.first);
  }
  return names;
}

bool CommandDictionary::LoadCommands(const base::DictionaryValue& json,
                                     const std::string& category,
                                     const CommandDictionary* base_commands,
                                     chromeos::ErrorPtr* error) {
  CommandMap new_defs;

  // |json| contains a list of nested objects with the following structure:
  // {"<pkg_name>": {"<cmd_name>": {"parameters": {object_schema}}, ...}, ...}
  // Iterate over packages
  base::DictionaryValue::Iterator package_iter(json);
  while (!package_iter.IsAtEnd()) {
    std::string package_name = package_iter.key();
    const base::DictionaryValue* package_value = nullptr;
    if (!package_iter.value().GetAsDictionary(&package_value)) {
      chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                                   errors::commands::kTypeMismatch,
                                   "Expecting an object for package '%s'",
                                   package_name.c_str());
      return false;
    }
    // Iterate over command definitions within the current package.
    base::DictionaryValue::Iterator command_iter(*package_value);
    while (!command_iter.IsAtEnd()) {
      std::string command_name = command_iter.key();
      if (command_name.empty()) {
        chromeos::Error::AddToPrintf(
            error, FROM_HERE, errors::commands::kDomain,
            errors::commands::kInvalidCommandName,
            "Unnamed command encountered in package '%s'",
            package_name.c_str());
        return false;
      }
      const base::DictionaryValue* command_def_json = nullptr;
      if (!command_iter.value().GetAsDictionary(&command_def_json)) {
        chromeos::Error::AddToPrintf(error, FROM_HERE,
                                     errors::commands::kDomain,
                                     errors::commands::kTypeMismatch,
                                     "Expecting an object for command '%s'",
                                     command_name.c_str());
        return false;
      }
      // Construct the compound command name as "pkg_name.cmd_name".
      std::string full_command_name =
          chromeos::string_utils::Join(".", package_name, command_name);

      const ObjectSchema* base_parameters_def = nullptr;
      const ObjectSchema* base_results_def = nullptr;
      // By default make it available to all clients.
      auto visibility = CommandDefinition::Visibility::GetAll();
      if (base_commands) {
        auto cmd = base_commands->FindCommand(full_command_name);
        if (cmd) {
          base_parameters_def = cmd->GetParameters();
          base_results_def = cmd->GetResults();
          visibility = cmd->GetVisibility();
        }

        // If the base command dictionary was provided but the command was not
        // found in it, this must be a custom (vendor) command. GCD spec states
        // that all custom command names must begin with "_". Let's enforce
        // this rule here.
        if (!cmd) {
          if (command_name.front() != '_') {
            chromeos::Error::AddToPrintf(
                error, FROM_HERE, errors::commands::kDomain,
                errors::commands::kInvalidCommandName,
                "The name of custom command '%s' in package '%s'"
                " must start with '_'",
                command_name.c_str(), package_name.c_str());
            return false;
          }
        }
      }

      auto parameters_schema = BuildObjectSchema(
          command_def_json,
          commands::attributes::kCommand_Parameters,
          base_parameters_def,
          full_command_name,
          error);
      if (!parameters_schema)
        return false;

      auto results_schema = BuildObjectSchema(
          command_def_json,
          commands::attributes::kCommand_Results,
          base_results_def,
          full_command_name,
          error);
      if (!results_schema)
        return false;

      std::string value;
      using commands::attributes::kCommand_Visibility;
      if (command_def_json->GetString(kCommand_Visibility, &value)) {
        if (!visibility.FromString(value, error)) {
          chromeos::Error::AddToPrintf(
              error, FROM_HERE, errors::commands::kDomain,
              errors::commands::kInvalidCommandVisibility,
              "Error parsing command '%s'", full_command_name.c_str());
          return false;
        }
      }

      std::unique_ptr<CommandDefinition> command_def{
        new CommandDefinition{category, std::move(parameters_schema),
                              std::move(results_schema)}
      };
      command_def->SetVisibility(visibility);
      new_defs.emplace(full_command_name, std::move(command_def));

      command_iter.Advance();
    }
    package_iter.Advance();
  }

  // Verify that newly loaded command definitions do not override existing
  // definitions in another category. This is unlikely, but we don't want to let
  // one vendor daemon to define the same commands already handled by another
  // daemon on the same device.
  for (const auto& pair : new_defs) {
    auto iter = definitions_.find(pair.first);
    CHECK(iter == definitions_.end())
        << "Definition for command '" << pair.first
        << "' overrides an earlier definition in category '"
        << iter->second->GetCategory().c_str() << "'";
  }

  // Now that we successfully loaded all the command definitions,
  // remove previous definitions of commands from the same category.
  std::vector<std::string> names = GetCommandNamesByCategory(category);
  for (const std::string& name : names)
    definitions_.erase(name);

  // Insert new definitions into the global map.
  for (auto& pair : new_defs)
    definitions_.emplace(pair.first, std::move(pair.second));
  return true;
}

std::unique_ptr<ObjectSchema> CommandDictionary::BuildObjectSchema(
    const base::DictionaryValue* command_def_json,
    const char* property_name,
    const ObjectSchema* base_def,
    const std::string& command_name,
    chromeos::ErrorPtr* error) {
  auto object_schema = ObjectSchema::Create();

  const base::DictionaryValue* schema_def = nullptr;
  if (!command_def_json->GetDictionaryWithoutPathExpansion(property_name,
                                                           &schema_def)) {
    chromeos::Error::AddToPrintf(
        error, FROM_HERE, errors::commands::kDomain,
        errors::commands::kPropertyMissing,
        "Command definition '%s' is missing property '%s'",
        command_name.c_str(),
        property_name);
    return {};
  }

  if (!object_schema->FromJson(schema_def, base_def, error)) {
    chromeos::Error::AddToPrintf(error, FROM_HERE,
                                 errors::commands::kDomain,
                                 errors::commands::kInvalidObjectSchema,
                                 "Invalid definition for command '%s'",
                                 command_name.c_str());
    return {};
  }

  return object_schema;
}

std::unique_ptr<base::DictionaryValue> CommandDictionary::GetCommandsAsJson(
    bool full_schema, chromeos::ErrorPtr* error) const {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  for (const auto& pair : definitions_) {
    std::unique_ptr<base::DictionaryValue> definition =
        pair.second->GetParameters()->ToJson(full_schema, error);
    if (!definition) {
      dict.reset();
      return dict;
    }
    auto cmd_name_parts = chromeos::string_utils::SplitAtFirst(pair.first, ".");
    std::string package_name = cmd_name_parts.first;
    std::string command_name = cmd_name_parts.second;
    base::DictionaryValue* package = nullptr;
    if (!dict->GetDictionaryWithoutPathExpansion(package_name, &package)) {
      // If this is the first time we encounter this package, create a JSON
      // object for it.
      package = new base::DictionaryValue;
      dict->SetWithoutPathExpansion(package_name, package);
    }
    base::DictionaryValue* command_def = new base::DictionaryValue;
    command_def->Set(commands::attributes::kCommand_Parameters,
                     definition.release());
    package->SetWithoutPathExpansion(command_name, command_def);
  }
  return dict;
}

const CommandDefinition* CommandDictionary::FindCommand(
    const std::string& command_name) const {
  auto pair = definitions_.find(command_name);
  return (pair != definitions_.end()) ? pair->second.get() : nullptr;
}

void CommandDictionary::Clear() {
  definitions_.clear();
}

}  // namespace buffet

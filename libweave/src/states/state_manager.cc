// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/states/state_manager.h"

#include <base/files/file_enumerator.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/values.h>

#include "libweave/src/json_error_codes.h"
#include "libweave/src/states/error_codes.h"
#include "libweave/src/states/state_change_queue_interface.h"
#include "libweave/src/string_utils.h"
#include "libweave/src/utils.h"

namespace weave {

StateManager::StateManager(StateChangeQueueInterface* state_change_queue)
    : state_change_queue_(state_change_queue) {
  CHECK(state_change_queue_) << "State change queue not specified";
}

StateManager::~StateManager() {}

void StateManager::AddOnChangedCallback(const base::Closure& callback) {
  on_changed_.push_back(callback);
  callback.Run();  // Force to read current state.
}

void StateManager::Startup() {
  LOG(INFO) << "Initializing StateManager.";

  // Load standard device state definition.
  base::FilePath base_state_file("/etc/buffet/base_state.schema.json");
  LOG(INFO) << "Loading standard state definition from "
            << base_state_file.value();
  CHECK(LoadBaseStateDefinition(base_state_file, nullptr))
      << "Failed to load the standard state definition file.";

  // Load component-specific device state definitions.
  base::FilePath device_state_dir("/etc/buffet/states");
  base::FileEnumerator enumerator(device_state_dir, false,
                                  base::FileEnumerator::FILES,
                                  FILE_PATH_LITERAL("*.schema.json"));
  base::FilePath json_file_path = enumerator.Next();
  while (!json_file_path.empty()) {
    LOG(INFO) << "Loading state definition from " << json_file_path.value();
    CHECK(LoadStateDefinition(json_file_path, nullptr))
        << "Failed to load the state definition file.";
    json_file_path = enumerator.Next();
  }

  // Load standard device state defaults.
  base::FilePath base_state_defaults("/etc/buffet/base_state.defaults.json");
  LOG(INFO) << "Loading base state defaults from "
            << base_state_defaults.value();
  CHECK(LoadStateDefaults(base_state_defaults, nullptr))
      << "Failed to load the base state defaults.";

  // Load component-specific device state defaults.
  base::FileEnumerator enumerator2(device_state_dir, false,
                                   base::FileEnumerator::FILES,
                                   FILE_PATH_LITERAL("*.defaults.json"));
  json_file_path = enumerator2.Next();
  while (!json_file_path.empty()) {
    LOG(INFO) << "Loading state defaults from " << json_file_path.value();
    CHECK(LoadStateDefaults(json_file_path, nullptr))
        << "Failed to load the state defaults.";
    json_file_path = enumerator2.Next();
  }

  for (const auto& cb : on_changed_)
    cb.Run();
}

std::unique_ptr<base::DictionaryValue> StateManager::GetStateValuesAsJson()
    const {
  std::unique_ptr<base::DictionaryValue> dict{new base::DictionaryValue};
  for (const auto& pair : packages_) {
    auto pkg_value = pair.second->GetValuesAsJson();
    CHECK(pkg_value);
    dict->SetWithoutPathExpansion(pair.first, pkg_value.release());
  }
  return dict;
}

bool StateManager::SetProperties(const base::DictionaryValue& property_set,
                                 ErrorPtr* error) {
  base::Time timestamp = base::Time::Now();
  bool all_success = true;
  for (base::DictionaryValue::Iterator it(property_set); !it.IsAtEnd();
       it.Advance()) {
    if (!SetPropertyValue(it.key(), it.value(), timestamp, error)) {
      // Remember that an error occurred but keep going and update the rest of
      // the properties if possible.
      all_success = false;
    }
  }
  for (const auto& cb : on_changed_)
    cb.Run();
  return all_success;
}

bool StateManager::SetPropertyValue(const std::string& full_property_name,
                                    const base::Value& value,
                                    const base::Time& timestamp,
                                    ErrorPtr* error) {
  auto parts = SplitAtFirst(full_property_name, ".", true);
  const std::string& package_name = parts.first;
  const std::string& property_name = parts.second;
  const bool split = (full_property_name.find(".") != std::string::npos);

  if (full_property_name.empty() || (split && property_name.empty())) {
    Error::AddTo(error, FROM_HERE, errors::state::kDomain,
                 errors::state::kPropertyNameMissing,
                 "Property name is missing");
    return false;
  }
  if (!split || package_name.empty()) {
    Error::AddTo(error, FROM_HERE, errors::state::kDomain,
                 errors::state::kPackageNameMissing,
                 "Package name is missing in the property name");
    return false;
  }
  StatePackage* package = FindPackage(package_name);
  if (package == nullptr) {
    Error::AddToPrintf(error, FROM_HERE, errors::state::kDomain,
                       errors::state::kPropertyNotDefined,
                       "Unknown state property package '%s'",
                       package_name.c_str());
    return false;
  }
  if (!package->SetPropertyValue(property_name, value, error))
    return false;

  ValueMap prop_set{{full_property_name, package->GetProperty(property_name)}};
  state_change_queue_->NotifyPropertiesUpdated(timestamp, prop_set);
  return true;
}

std::pair<StateChangeQueueInterface::UpdateID, std::vector<StateChange>>
StateManager::GetAndClearRecordedStateChanges() {
  return std::make_pair(state_change_queue_->GetLastStateChangeId(),
                        state_change_queue_->GetAndClearRecordedStateChanges());
}

void StateManager::NotifyStateUpdatedOnServer(
    StateChangeQueueInterface::UpdateID id) {
  state_change_queue_->NotifyStateUpdatedOnServer(id);
}

bool StateManager::LoadStateDefinition(const base::DictionaryValue& json,
                                       const std::string& category,
                                       ErrorPtr* error) {
  base::DictionaryValue::Iterator iter(json);
  while (!iter.IsAtEnd()) {
    std::string package_name = iter.key();
    if (package_name.empty()) {
      Error::AddTo(error, FROM_HERE, kErrorDomain, kInvalidPackageError,
                   "State package name is empty");
      return false;
    }
    const base::DictionaryValue* package_dict = nullptr;
    if (!iter.value().GetAsDictionary(&package_dict)) {
      Error::AddToPrintf(error, FROM_HERE, errors::json::kDomain,
                         errors::json::kObjectExpected,
                         "State package '%s' must be an object",
                         package_name.c_str());
      return false;
    }
    StatePackage* package = FindOrCreatePackage(package_name);
    CHECK(package) << "Unable to create state package " << package_name;
    if (!package->AddSchemaFromJson(package_dict, error))
      return false;
    iter.Advance();
  }
  if (category != kDefaultCategory)
    categories_.insert(category);

  return true;
}

bool StateManager::LoadStateDefinition(const base::FilePath& json_file_path,
                                       ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json =
      LoadJsonDict(json_file_path, error);
  if (!json)
    return false;
  std::string category = json_file_path.BaseName().RemoveExtension().value();
  if (category == kDefaultCategory) {
    Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kInvalidCategoryError,
                       "Invalid state category specified in '%s'",
                       json_file_path.value().c_str());
    return false;
  }

  if (!LoadStateDefinition(*json, category, error)) {
    Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kFileReadError,
                       "Failed to load file '%s'",
                       json_file_path.value().c_str());
    return false;
  }
  return true;
}

bool StateManager::LoadBaseStateDefinition(const base::FilePath& json_file_path,
                                           ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json =
      LoadJsonDict(json_file_path, error);
  if (!json)
    return false;
  if (!LoadStateDefinition(*json, kDefaultCategory, error)) {
    Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kFileReadError,
                       "Failed to load file '%s'",
                       json_file_path.value().c_str());
    return false;
  }
  return true;
}

bool StateManager::LoadStateDefaults(const base::DictionaryValue& json,
                                     ErrorPtr* error) {
  base::DictionaryValue::Iterator iter(json);
  while (!iter.IsAtEnd()) {
    std::string package_name = iter.key();
    if (package_name.empty()) {
      Error::AddTo(error, FROM_HERE, kErrorDomain, kInvalidPackageError,
                   "State package name is empty");
      return false;
    }
    const base::DictionaryValue* package_dict = nullptr;
    if (!iter.value().GetAsDictionary(&package_dict)) {
      Error::AddToPrintf(error, FROM_HERE, errors::json::kDomain,
                         errors::json::kObjectExpected,
                         "State package '%s' must be an object",
                         package_name.c_str());
      return false;
    }
    StatePackage* package = FindPackage(package_name);
    if (package == nullptr) {
      Error::AddToPrintf(error, FROM_HERE, errors::json::kDomain,
                         errors::json::kObjectExpected,
                         "Providing values for undefined state package '%s'",
                         package_name.c_str());
      return false;
    }
    if (!package->AddValuesFromJson(package_dict, error))
      return false;
    iter.Advance();
  }
  return true;
}

bool StateManager::LoadStateDefaults(const base::FilePath& json_file_path,
                                     ErrorPtr* error) {
  std::unique_ptr<const base::DictionaryValue> json =
      LoadJsonDict(json_file_path, error);
  if (!json)
    return false;
  if (!LoadStateDefaults(*json, error)) {
    Error::AddToPrintf(error, FROM_HERE, kErrorDomain, kFileReadError,
                       "Failed to load file '%s'",
                       json_file_path.value().c_str());
    return false;
  }
  return true;
}

StatePackage* StateManager::FindPackage(const std::string& package_name) {
  auto it = packages_.find(package_name);
  return (it != packages_.end()) ? it->second.get() : nullptr;
}

StatePackage* StateManager::FindOrCreatePackage(
    const std::string& package_name) {
  StatePackage* package = FindPackage(package_name);
  if (package == nullptr) {
    std::unique_ptr<StatePackage> new_package{new StatePackage(package_name)};
    package = packages_.emplace(package_name, std::move(new_package))
                  .first->second.get();
  }
  return package;
}

}  // namespace weave

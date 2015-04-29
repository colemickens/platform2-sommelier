// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_definition.h"

#include <vector>

#include <chromeos/errors/error.h>
#include <chromeos/strings/string_utils.h>

#include "buffet/commands/schema_constants.h"

namespace buffet {

bool CommandDefinition::Visibility::FromString(const std::string& str,
                                               chromeos::ErrorPtr* error) {
  // This special case is useful for places where we want to make a command
  // to ALL clients, even if new clients are added in the future.
  if (str == commands::attributes::kCommand_Visibility_All) {
    local = true;
    cloud = true;
    return true;
  }

  // Clear any bits first.
  local = false;
  cloud = false;
  if (str == commands::attributes::kCommand_Visibility_None)
    return true;

  for (const std::string& value : chromeos::string_utils::Split(str, ",")) {
    if (value == commands::attributes::kCommand_Visibility_Local) {
      local = true;
    } else if (value == commands::attributes::kCommand_Visibility_Cloud) {
      cloud = true;
    } else {
      chromeos::Error::AddToPrintf(
          error, FROM_HERE, errors::commands::kDomain,
          errors::commands::kInvalidPropValue,
          "Invalid command visibility value '%s'", value.c_str());
      return false;
    }
  }
  return true;
}

std::string CommandDefinition::Visibility::ToString() const {
  if (local && cloud)
    return commands::attributes::kCommand_Visibility_All;
  if (!local && !cloud)
    return commands::attributes::kCommand_Visibility_None;
  if (local)
    return commands::attributes::kCommand_Visibility_Local;
  return commands::attributes::kCommand_Visibility_Cloud;
}

CommandDefinition::CommandDefinition(
    const std::string& category,
    std::unique_ptr<const ObjectSchema> parameters,
    std::unique_ptr<const ObjectSchema> progress,
    std::unique_ptr<const ObjectSchema> results)
    : category_{category},
      parameters_{std::move(parameters)},
      progress_{std::move(progress)},
      results_{std::move(results)} {
  // Set to be available to all clients by default.
  visibility_ = Visibility::GetAll();
}

void CommandDefinition::SetVisibility(const Visibility& visibility) {
  visibility_ = visibility;
}

}  // namespace buffet

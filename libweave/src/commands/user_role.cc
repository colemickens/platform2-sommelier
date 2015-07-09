// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/user_role.h"

#include <chromeos/errors/error.h>

#include "libweave/src/commands/enum_to_string.h"
#include "libweave/src/commands/schema_constants.h"

namespace weave {

template <>
const EnumToString<UserRole>::Map EnumToString<UserRole>::kMap[] = {
    {UserRole::kViewer, commands::attributes::kCommand_Role_Viewer},
    {UserRole::kUser, commands::attributes::kCommand_Role_User},
    {UserRole::kOwner, commands::attributes::kCommand_Role_Owner},
    {UserRole::kManager, commands::attributes::kCommand_Role_Manager},
};

std::string ToString(UserRole role) {
  return EnumToString<UserRole>::FindNameById(role);
}

bool FromString(const std::string& str,
                UserRole* role,
                chromeos::ErrorPtr* error) {
  if (EnumToString<UserRole>::FindIdByName(str, role))
    return true;
  chromeos::Error::AddToPrintf(error, FROM_HERE, errors::commands::kDomain,
                               errors::commands::kInvalidPropValue,
                               "Invalid role: '%s'", str.c_str());
  return false;
}

}  // namespace weave

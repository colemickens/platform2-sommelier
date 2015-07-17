// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/user_role.h"

#include <chromeos/errors/error.h>

#include "libweave/src/commands/schema_constants.h"
#include "weave/enum_to_string.h"

namespace weave {

namespace {

const EnumToStringMap<UserRole>::Map kMap[] = {
    {UserRole::kViewer, commands::attributes::kCommand_Role_Viewer},
    {UserRole::kUser, commands::attributes::kCommand_Role_User},
    {UserRole::kOwner, commands::attributes::kCommand_Role_Owner},
    {UserRole::kManager, commands::attributes::kCommand_Role_Manager},
};

}  // namespace

template <>
EnumToStringMap<UserRole>::EnumToStringMap()
    : EnumToStringMap(kMap) {}

}  // namespace weave

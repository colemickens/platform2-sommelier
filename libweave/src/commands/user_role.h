// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_COMMANDS_USER_ROLE_H_
#define LIBWEAVE_SRC_COMMANDS_USER_ROLE_H_

#include <string>
#include <chromeos/errors/error.h>

namespace weave {

enum class UserRole {
  kViewer,
  kUser,
  kManager,
  kOwner,
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_COMMANDS_USER_ROLE_H_

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_USERDB_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_USERDB_UTILS_H_

#include <sys/types.h>

#include <string>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <chromeos/chromeos_export.h>

namespace chromeos {
namespace userdb {

// Looks up the UID and GID corresponding to |user|. Returns true on success.
// Passing nullptr for |uid| or |gid| causes them to be ignored.
CHROMEOS_EXPORT bool GetUserInfo(
    const std::string& user, uid_t* uid, gid_t* gid) WARN_UNUSED_RESULT;

// Looks up the GID corresponding to |group|. Returns true on success.
// Passing nullptr for |gid| causes it to be ignored.
CHROMEOS_EXPORT bool GetGroupInfo(
    const std::string& group, gid_t* gid) WARN_UNUSED_RESULT;

}  // namespace userdb
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_USERDB_UTILS_H_

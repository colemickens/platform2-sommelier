// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/userdb.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <chromeos/userdb_utils.h>

namespace soma {
namespace parser {

bool Userdb::ResolveUser(const std::string& user, uid_t* uid) {
  DCHECK(uid);
  // If user is just a stringified uid.
  if (base::StringToUint(user, uid))
    return true;
  return chromeos::userdb::GetUserInfo(user, uid, nullptr);
}

bool Userdb::ResolveGroup(const std::string& group, gid_t* gid) {
  DCHECK(gid);
  // If group is just a stringified gid.
  if (base::StringToUint(group, gid))
    return true;
  return chromeos::userdb::GetGroupInfo(group, gid);
}

}  // namespace parser
}  // namespace soma

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "soma/lib/soma/fake_userdb.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>

namespace soma {
namespace parser {

FakeUserdb::FakeUserdb() : next_uid_(1), next_gid_(1) {
}
FakeUserdb::~FakeUserdb() = default;

bool FakeUserdb::ResolveUser(const std::string& user, uid_t* uid) {
  DCHECK(uid);
  if (base::StringToUint(user, uid))
    return true;
  if (user_mappings_.count(user) == 0) {
    if (whitelisted_namespace_.empty() ||
        !StartsWithASCII(user, whitelisted_namespace_, false)) {
      return false;
    }
    user_mappings_[user] = ++next_uid_;
  }
  *uid = user_mappings_.at(user);
  return true;
}

bool FakeUserdb::ResolveGroup(const std::string& group, gid_t* gid) {
  DCHECK(gid);
  if (base::StringToUint(group, gid))
    return true;
  if (group_mappings_.count(group) == 0) {
    if (whitelisted_namespace_.empty() ||
        !StartsWithASCII(group, whitelisted_namespace_, false)) {
      return false;
    }
    group_mappings_[group] = ++next_gid_;
  }
  *gid = group_mappings_.at(group);
  return true;
}

}  // namespace parser
}  // namespace soma

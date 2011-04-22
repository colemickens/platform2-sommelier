// Copyright (c) 2009-2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "user_oldest_activity_timestamp_cache.h"

namespace cryptohome {

void UserOldestActivityTimestampCache::AddExistingUser(
    const FilePath& vault, base::Time timestamp) {
  users_timestamp_.insert(std::make_pair(timestamp, vault));
  if (latest_known_timestamp_ > timestamp ||
      latest_known_timestamp_.is_null()) {
    latest_known_timestamp_ = timestamp;
  }
}

void UserOldestActivityTimestampCache::UpdateExistingUser(
    const FilePath& vault, base::Time timestamp) {
  for (UsersTimestamp::iterator i = users_timestamp_.begin();
       i != users_timestamp_.end(); ++i) {
    if (i->second == vault) {
      users_timestamp_.erase(i);
      break;
    }
  }
  AddExistingUser(vault, timestamp);
}

void UserOldestActivityTimestampCache::AddExistingUserNotime(
    const FilePath& vault) {
  users_timestamp_.insert(std::make_pair(base::Time(), vault));
}

FilePath UserOldestActivityTimestampCache::RemoveOldestUser() {
  FilePath vault;
  if (!users_timestamp_.empty()) {
    vault = users_timestamp_.begin()->second;
    base::Time timestamp = users_timestamp_.begin()->first;
    users_timestamp_.erase(users_timestamp_.begin());
    if (latest_known_timestamp_ == timestamp) {
      latest_known_timestamp_ = users_timestamp_.empty() ?
          base::Time() : users_timestamp_.begin()->first;
    }
  }
  return vault;
}

}  // namespace cryptohome

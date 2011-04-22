// Copyright (c) 2009-2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_USER_OLDEST_ACTIVITY_TIMESTAMP_CACHE_H_
#define CRYPTOHOME_USER_OLDEST_ACTIVITY_TIMESTAMP_CACHE_H_

#include <map>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/time.h>

namespace cryptohome {

// Cache of last access timestamp for existing users.
class UserOldestActivityTimestampCache {
 public:
  UserOldestActivityTimestampCache() { }
  virtual ~UserOldestActivityTimestampCache() { }

  virtual bool empty() const { return users_timestamp_.empty(); }

  // Adds a user to the cache with specified latest activity timestamp.
  virtual void AddExistingUser(const FilePath& vault, base::Time timestamp);

  // Updates a user in the cache with specified latest activity timestamp.
  virtual void UpdateExistingUser(const FilePath& vault, base::Time timestamp);

  // Adds a user to the cache without latest activity timestamp. Such
  // users are considered older than any existing user with timestamp.
  virtual void AddExistingUserNotime(const FilePath& vault);

  // Timestamp of the oldest user in the cache. May be null (check for
  // is_null) if there is no user with definite timestamp.
  virtual base::Time latest_known_timestamp() const {
    return latest_known_timestamp_;
  }

  // Removes the oldest user stored in the cache. Users without
  // a timestamp are removed first.
  virtual FilePath RemoveOldestUser();

 private:
  typedef std::multimap<base::Time, FilePath> UsersTimestamp;
  UsersTimestamp users_timestamp_;
  base::Time latest_known_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(UserOldestActivityTimestampCache);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_USER_OLDEST_ACTIVITY_TIMESTAMP_CACHE_H_

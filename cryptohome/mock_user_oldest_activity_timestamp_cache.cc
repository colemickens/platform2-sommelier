// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_user_oldest_activity_timestamp_cache.h"

namespace cryptohome {

using ::testing::Invoke;

MockUserOldestActivityTimestampCache::MockUserOldestActivityTimestampCache() {
  ON_CALL(*this, oldest_known_timestamp())
      .WillByDefault(
        Invoke(this,
          &MockUserOldestActivityTimestampCache::StubOldestKnownTimestamp));
  ON_CALL(*this, RemoveOldestUser())
      .WillByDefault(
        Invoke(this,
          &MockUserOldestActivityTimestampCache::StubRemoveOldestUser));
}
MockUserOldestActivityTimestampCache::~MockUserOldestActivityTimestampCache() {}

}  // namespace cryptohome

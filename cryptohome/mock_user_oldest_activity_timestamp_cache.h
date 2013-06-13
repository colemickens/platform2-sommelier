// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_USER_OLDEST_ACTIVITY_TIMESTAMP_CACHE_H_
#define MOCK_USER_OLDEST_ACTIVITY_TIMESTAMP_CACHE_H_

#include "user_oldest_activity_timestamp_cache.h"

#include <chromeos/secure_blob.h>
#include <gmock/gmock.h>

namespace cryptohome {

class MockUserOldestActivityTimestampCache :
    public UserOldestActivityTimestampCache {
 public:
  MockUserOldestActivityTimestampCache();
  virtual ~MockUserOldestActivityTimestampCache();

  MOCK_METHOD0(Initialize, void(void));
  MOCK_CONST_METHOD0(initialized, bool(void));
  MOCK_METHOD2(AddExistingUser, void(const FilePath&, base::Time));
  MOCK_METHOD2(UpdateExistingUser, void(const FilePath&, base::Time));
  MOCK_METHOD1(AddExistingUserNotime, void(const FilePath&));
  MOCK_CONST_METHOD0(oldest_known_timestamp, base::Time(void));
  MOCK_METHOD0(RemoveOldestUser, FilePath(void));

 private:
   base::Time StubOldestKnownTimestamp() const {
     return base::Time();  // null
   }

   FilePath StubRemoveOldestUser() {
     return FilePath("/SATURATED/REMOVE/OLDEST/USER");
   }

};
}  // namespace cryptohome

#endif  /* !MOCK_USER_OLDEST_ACTIVITY_TIMESTAMP_CACHE_H_ */

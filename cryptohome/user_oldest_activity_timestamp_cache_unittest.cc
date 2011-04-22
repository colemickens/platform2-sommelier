// Copyright (c) 2009-2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsersLatestActivityTimestampCache.

#include "user_oldest_activity_timestamp_cache.h"

#include <base/logging.h>
#include <chromeos/utility.h>
#include <gtest/gtest.h>

namespace cryptohome {
using std::string;

TEST(UserOldestActivityTimestampCache, Sequential) {
  const base::Time::Exploded jan1st2011_exploded = { 2011, 1, 6, 1 };
  const base::Time time_jan1 = base::Time::FromUTCExploded(jan1st2011_exploded);
  const base::Time::Exploded feb1st2011_exploded = { 2011, 2, 2, 1 };
  const base::Time time_feb1 = base::Time::FromUTCExploded(feb1st2011_exploded);
  const base::Time::Exploded mar1st2011_exploded = { 2011, 3, 2, 1 };
  const base::Time time_mar1 = base::Time::FromUTCExploded(mar1st2011_exploded);

  UserOldestActivityTimestampCache cache;
  EXPECT_TRUE(cache.empty());

  // Fill the cache with users with different (or no) timestamp.
  // Check that the latest timestamp is actually latest.
  cache.AddExistingUserNotime(FilePath("a"));
  EXPECT_FALSE(cache.empty());
  EXPECT_TRUE(cache.latest_known_timestamp().is_null());

  cache.AddExistingUser(FilePath("b"), time_mar1);
  EXPECT_FALSE(cache.empty());
  EXPECT_FALSE(cache.latest_known_timestamp().is_null());
  EXPECT_TRUE(time_mar1 == cache.latest_known_timestamp());

  cache.AddExistingUser(FilePath("c"), time_jan1);
  EXPECT_TRUE(time_jan1 == cache.latest_known_timestamp());

  cache.AddExistingUser(FilePath("d"), time_feb1);
  EXPECT_TRUE(time_jan1 == cache.latest_known_timestamp());
  cache.UpdateExistingUser(FilePath("d"), time_mar1);
  EXPECT_TRUE(time_jan1 == cache.latest_known_timestamp());

  cache.AddExistingUserNotime(FilePath("e"));
  EXPECT_TRUE(time_jan1 == cache.latest_known_timestamp());

  // Remove users one by one, check the remaining latest timestamp.
  EXPECT_EQ("a", cache.RemoveOldestUser().value());
  EXPECT_TRUE(time_jan1 == cache.latest_known_timestamp());

  EXPECT_EQ("e", cache.RemoveOldestUser().value());
  EXPECT_TRUE(time_jan1 == cache.latest_known_timestamp());

  EXPECT_EQ("c", cache.RemoveOldestUser().value());
  EXPECT_TRUE(time_mar1 == cache.latest_known_timestamp());

  EXPECT_EQ("b", cache.RemoveOldestUser().value());
  EXPECT_TRUE(time_mar1 == cache.latest_known_timestamp());

  EXPECT_EQ("d", cache.RemoveOldestUser().value());
  EXPECT_TRUE(cache.latest_known_timestamp().is_null());
  EXPECT_TRUE(cache.empty());
}

} // namespace cryptohome

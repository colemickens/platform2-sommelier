// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/utility.h"

#include <base/stl_util.h>
#include <gtest/gtest.h>

using std::map;
using std::set;

namespace wimax_manager {

class UtilityTest : public testing::Test {};

TEST_F(UtilityTest, GetKeysOfMapWithEmptyMap) {
  map<int, char> empty_map;
  set<int> keys = GetKeysOfMap(empty_map);
  EXPECT_TRUE(keys.empty());
}

TEST_F(UtilityTest, GetKeysOfMap) {
  map<int, char> test_map;
  test_map[2] = 'b';
  test_map[1] = 'a';
  test_map[3] = 'c';

  set<int> keys = GetKeysOfMap(test_map);
  EXPECT_EQ(3, keys.size());
  EXPECT_TRUE(base::ContainsKey(keys, 1));
  EXPECT_TRUE(base::ContainsKey(keys, 2));
  EXPECT_TRUE(base::ContainsKey(keys, 3));
  EXPECT_FALSE(base::ContainsKey(keys, 4));
}

TEST_F(UtilityTest, RemoveKeysFromMapWithEmptySetOfKeys) {
  set<int> keys_to_remove;
  map<int, char> test_map;
  test_map[2] = 'b';
  test_map[1] = 'a';
  test_map[3] = 'c';

  RemoveKeysFromMap(&test_map, keys_to_remove);
  EXPECT_EQ(3, test_map.size());
  EXPECT_EQ('a', test_map[1]);
  EXPECT_EQ('b', test_map[2]);
  EXPECT_EQ('c', test_map[3]);
}

TEST_F(UtilityTest, RemoveKeysFromMapWithEmptyMap) {
  set<int> keys_to_remove;
  keys_to_remove.insert(1);
  keys_to_remove.insert(4);

  map<int, char> test_map;
  RemoveKeysFromMap(&test_map, keys_to_remove);
  EXPECT_TRUE(test_map.empty());
}

TEST_F(UtilityTest, RemoveKeysFromMap) {
  set<int> keys_to_remove;
  keys_to_remove.insert(1);
  keys_to_remove.insert(4);

  map<int, char> test_map;
  test_map[2] = 'b';
  test_map[1] = 'a';
  test_map[3] = 'c';

  RemoveKeysFromMap(&test_map, keys_to_remove);
  EXPECT_EQ(2, test_map.size());
  EXPECT_FALSE(base::ContainsKey(test_map, 1));
  EXPECT_FALSE(base::ContainsKey(test_map, 4));
  EXPECT_EQ('b', test_map[2]);
  EXPECT_EQ('c', test_map[3]);
}

}  // namespace wimax_manager

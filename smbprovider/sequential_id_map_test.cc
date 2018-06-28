// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <gtest/gtest.h>

#include "smbprovider/sequential_id_map.h"

namespace smbprovider {

class SequentialIdMapTest : public testing::Test {
 public:
  SequentialIdMapTest() = default;
  ~SequentialIdMapTest() override = default;

 protected:
  void ExpectFound(int32_t id, std::string expected) {
    auto iter = map_.Find(id);
    EXPECT_NE(map_.End(), iter);
    EXPECT_EQ(expected, iter->second);
  }

  void ExpectNotFound(int32_t id) {
    auto iter = map_.Find(id);
    EXPECT_EQ(map_.End(), iter);
    EXPECT_FALSE(map_.Contains(id));
  }

  SequentialIdMap<const std::string> map_;
  DISALLOW_COPY_AND_ASSIGN(SequentialIdMapTest);
};

TEST_F(SequentialIdMapTest, FindOnEmpty) {
  ExpectNotFound(0);
}

TEST_F(SequentialIdMapTest, TestInsertandFind) {
  const std::string expected = "Foo";
  const int32_t id = map_.Insert(expected);

  // First id is 0.
  EXPECT_EQ(0, id);
  ExpectFound(id, expected);
}

TEST_F(SequentialIdMapTest, TestInsertAndContains) {
  const std::string expected = "Foo";
  const int32_t id = map_.Insert(expected);

  // First id is 0.
  EXPECT_EQ(0, id);
  EXPECT_TRUE(map_.Contains(id));
  EXPECT_FALSE(map_.Contains(id + 1));
}

TEST_F(SequentialIdMapTest, TestInsertandFindNonExistant) {
  const std::string expected = "Foo";
  const int32_t id = map_.Insert(expected);

  // First id is 0.
  EXPECT_EQ(0, id);
  ExpectFound(id, expected);
  ExpectNotFound(id + 1);
}

TEST_F(SequentialIdMapTest, TestInsertMultipleAndFind) {
  const std::string expected1 = "Foo1";
  const std::string expected2 = "Foo2";
  const int32_t id1 = map_.Insert(expected1);
  const int32_t id2 = map_.Insert(expected2);

  // First id is 0, second is 1.
  EXPECT_EQ(0, id1);
  EXPECT_EQ(1, id2);

  ExpectFound(id1, expected1);
  ExpectFound(id2, expected2);
}

}  // namespace smbprovider

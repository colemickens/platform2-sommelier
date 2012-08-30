// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_value_store.h"

#include <gtest/gtest.h>

using std::string;
using testing::Test;

namespace shill {

class KeyValueStoreTest : public Test {
 public:
  KeyValueStoreTest() {}

 protected:
  KeyValueStore store_;
};

TEST_F(KeyValueStoreTest, Bool) {
  const string kKey("foo");
  const bool kDefaultValue = true;
  const bool kValue = false;
  EXPECT_FALSE(store_.ContainsBool(kKey));
  EXPECT_EQ(kDefaultValue, store_.LookupBool(kKey, kDefaultValue));
  store_.SetBool(kKey, kValue);
  EXPECT_TRUE(store_.ContainsBool(kKey));
  // TODO: investigate if a newer version of gtest handles EXPECT_EQ for bools
  // in a manner that gcc 4.7 is happy with. (Inproper conversion from "false"
  // to "NULL".)
  EXPECT_EQ(static_cast<int>(kValue),
            static_cast<int>(store_.LookupBool(kKey, kDefaultValue)));
  EXPECT_EQ(static_cast<int>(kValue),
            static_cast<int>(store_.GetBool(kKey)));
}

TEST_F(KeyValueStoreTest, Int) {
  const string kKey("foo");
  const int kValue = 456;
  EXPECT_FALSE(store_.ContainsInt(kKey));
  store_.SetInt(kKey, kValue);
  EXPECT_TRUE(store_.ContainsInt(kKey));
  EXPECT_EQ(kValue, store_.GetInt(kKey));
  store_.RemoveInt(kKey);
  EXPECT_FALSE(store_.ContainsInt(kKey));
}

TEST_F(KeyValueStoreTest, String) {
  const string kKey("foo");
  const string kDefaultValue("bar");
  const string kValue("baz");
  EXPECT_FALSE(store_.ContainsString(kKey));
  EXPECT_EQ(kDefaultValue, store_.LookupString(kKey, kDefaultValue));
  store_.SetString(kKey, kValue);
  EXPECT_TRUE(store_.ContainsString(kKey));
  EXPECT_EQ(kValue, store_.LookupString(kKey, kDefaultValue));
  EXPECT_EQ(kValue, store_.GetString(kKey));
  store_.RemoveString(kKey);
  EXPECT_FALSE(store_.ContainsString(kKey));
  EXPECT_EQ(kDefaultValue, store_.LookupString(kKey, kDefaultValue));
}

TEST_F(KeyValueStoreTest, Uint) {
  const string kKey("foo");
  const uint32 kValue = 456;
  EXPECT_FALSE(store_.ContainsUint(kKey));
  store_.SetUint(kKey, kValue);
  EXPECT_TRUE(store_.ContainsUint(kKey));
  EXPECT_EQ(kValue, store_.GetUint(kKey));
}

TEST_F(KeyValueStoreTest, DoubleRemove) {
  const string kKey("foo");
  // Make sure we don't get an exception/infinite loop if we do a
  // "Remove()" when the key does not exist.
  store_.RemoveInt(kKey);
  store_.RemoveInt(kKey);
  store_.RemoveString(kKey);
  store_.RemoveString(kKey);
}

TEST_F(KeyValueStoreTest, Clear) {
  const string kBoolKey("foo");
  const bool kBoolValue = true;
  store_.SetBool(kBoolKey, kBoolValue);
  const string kIntKey("bar");
  const int kIntValue = 123;
  store_.SetInt(kIntKey, kIntValue);
  const string kStringKey("baz");
  const string kStringValue("string");
  store_.SetString(kStringKey, kStringValue);
  const string kUintKey("bun");
  const uint32 kUintValue = 456;
  store_.SetUint(kUintKey, kUintValue);

  EXPECT_TRUE(store_.ContainsBool(kBoolKey));
  EXPECT_TRUE(store_.ContainsInt(kIntKey));
  EXPECT_TRUE(store_.ContainsString(kStringKey));
  EXPECT_TRUE(store_.ContainsUint(kUintKey));
  store_.Clear();
  EXPECT_FALSE(store_.ContainsBool(kBoolKey));
  EXPECT_FALSE(store_.ContainsInt(kIntKey));
  EXPECT_FALSE(store_.ContainsString(kStringKey));
  EXPECT_FALSE(store_.ContainsUint(kUintKey));
}

}  // namespace shill

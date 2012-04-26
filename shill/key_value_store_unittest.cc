// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_value_store.h"

#include <gtest/gtest.h>

using testing::Test;

namespace shill {

class KeyValueStoreTest : public Test {
 public:
  KeyValueStoreTest() {}

 protected:
  KeyValueStore store_;
};

TEST_F(KeyValueStoreTest, LookupBool) {
  EXPECT_FALSE(store_.LookupBool("foo", false));
  store_.SetBool("foo", true);
  EXPECT_TRUE(store_.LookupBool("foo", false));
  EXPECT_TRUE(store_.LookupBool("moo", true));
  store_.SetBool("moo", false);
  EXPECT_FALSE(store_.LookupBool("moo", true));
}

TEST_F(KeyValueStoreTest, LookupString) {
  EXPECT_EQ("bar", store_.LookupString("foo", "bar"));
  store_.SetString("foo", "zoo");
  EXPECT_EQ("zoo", store_.LookupString("foo", "bar"));
}

TEST_F(KeyValueStoreTest, RemoveString) {
  const std::string kKey("foo");
  store_.SetString(kKey, "zoo");
  EXPECT_EQ("zoo", store_.LookupString(kKey, "bar"));
  store_.RemoveString(kKey);
  EXPECT_EQ("bar", store_.LookupString(kKey, "bar"));
  // Make sure we don't get an exception/infinite loop if we do a
  // "RemoveString()" when the key does not exist.
  store_.RemoveString(kKey);
}

}  // namespace shill

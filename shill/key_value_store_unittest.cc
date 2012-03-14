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

TEST_F(KeyValueStoreTest, LookupString) {
  EXPECT_EQ("bar", store_.LookupString("foo", "bar"));
  store_.SetString("foo", "zoo");
  EXPECT_EQ("zoo", store_.LookupString("foo", "bar"));
}

}  // namespace shill

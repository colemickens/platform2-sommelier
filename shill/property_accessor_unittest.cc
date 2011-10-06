// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/property_accessor.h"

#include <limits>
#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/error.h"

using std::map;
using std::string;
using std::vector;
using ::testing::Return;
using ::testing::Test;

namespace shill {

TEST(PropertyAccessorTest, SignedIntCorrectness) {
  int32 int_store = 0;
  {
    Error error;
    Int32Accessor accessor(new PropertyAccessor<int32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get());

    int32 expected_int32 = 127;
    accessor->Set(expected_int32, &error);
    ASSERT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_int32, accessor->Get());

    int_store = std::numeric_limits<int32>::max();
    EXPECT_EQ(std::numeric_limits<int32>::max(), accessor->Get());
  }
  {
    Error error;
    Int32Accessor accessor(new ConstPropertyAccessor<int32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get());

    int32 expected_int32 = 127;
    accessor->Set(expected_int32, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(int_store, accessor->Get());

    int_store = std::numeric_limits<int32>::max();
    EXPECT_EQ(std::numeric_limits<int32>::max(), accessor->Get());
  }
}

TEST(PropertyAccessorTest, UnsignedIntCorrectness) {
  uint32 int_store = 0;
  {
    Error error;
    Uint32Accessor accessor(new PropertyAccessor<uint32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get());

    uint32 expected_uint32 = 127;
    accessor->Set(expected_uint32, &error);
    ASSERT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_uint32, accessor->Get());

    int_store = std::numeric_limits<uint32>::max();
    EXPECT_EQ(std::numeric_limits<uint32>::max(), accessor->Get());
  }
  {
    Error error;
    Uint32Accessor accessor(new ConstPropertyAccessor<uint32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get());

    uint32 expected_uint32 = 127;
    accessor->Set(expected_uint32, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(int_store, accessor->Get());

    int_store = std::numeric_limits<uint32>::max();
    EXPECT_EQ(std::numeric_limits<uint32>::max(), accessor->Get());
  }
}

TEST(PropertyAccessorTest, StringCorrectness) {
  string storage;
  {
    Error error;
    StringAccessor accessor(new PropertyAccessor<string>(&storage));
    EXPECT_EQ(storage, accessor->Get());

    string expected_string("what");
    accessor->Set(expected_string, &error);
    ASSERT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_string, accessor->Get());

    storage = "nooooo";
    EXPECT_EQ(storage, accessor->Get());
  }
  {
    Error error;
    StringAccessor accessor(new ConstPropertyAccessor<string>(&storage));
    EXPECT_EQ(storage, accessor->Get());

    string expected_string("what");
    accessor->Set(expected_string, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(storage, accessor->Get());

    storage = "nooooo";
    EXPECT_EQ(storage, accessor->Get());
  }
}

}  // namespace shill

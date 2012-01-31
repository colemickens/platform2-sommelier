// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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
    int32 orig_value = int_store;
    Int32Accessor accessor(new PropertyAccessor<int32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get(&error));

    int32 expected_int32 = 127;
    accessor->Set(expected_int32, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_int32, accessor->Get(&error));

    accessor->Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, accessor->Get(&error));

    int_store = std::numeric_limits<int32>::max();
    EXPECT_EQ(std::numeric_limits<int32>::max(), accessor->Get(&error));
  }
  {
    Error error;
    Int32Accessor accessor(new ConstPropertyAccessor<int32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get(&error));

    int32 expected_int32 = 127;
    accessor->Set(expected_int32, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(int_store, accessor->Get(&error));

    int_store = std::numeric_limits<int32>::max();
    EXPECT_EQ(std::numeric_limits<int32>::max(), accessor->Get(&error));
  }
  {
    Error error;
    Int32Accessor accessor(new ConstPropertyAccessor<int32>(&int_store));
    accessor->Clear(&error);
    ASSERT_FALSE(error.IsSuccess());
  }
  {
    Error error;
    Int32Accessor accessor(new WriteOnlyPropertyAccessor<int32>(&int_store));
    accessor->Get(&error);
    EXPECT_TRUE(error.IsFailure());
    EXPECT_EQ(Error::kPermissionDenied, error.type());
  }
  {
    Error error;
    int32 expected_int32 = 127;
    WriteOnlyPropertyAccessor<int32> accessor(&int_store);
    accessor.Set(expected_int32, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_int32, *accessor.property_);
    EXPECT_EQ(int32(), accessor.Get(&error));
    ASSERT_FALSE(error.IsSuccess());

    int_store = std::numeric_limits<int32>::max();
    EXPECT_EQ(std::numeric_limits<int32>::max(), *accessor.property_);
  }
  {
    Error error;
    int32 orig_value = int_store = 0;
    WriteOnlyPropertyAccessor<int32> accessor(&int_store);

    accessor.Set(127, &error);
    accessor.Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, *accessor.property_);
  }
}

TEST(PropertyAccessorTest, UnsignedIntCorrectness) {
  uint32 int_store = 0;
  {
    Error error;
    uint32 orig_value = int_store;
    Uint32Accessor accessor(new PropertyAccessor<uint32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get(&error));

    uint32 expected_uint32 = 127;
    accessor->Set(expected_uint32, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_uint32, accessor->Get(&error));

    accessor->Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, accessor->Get(&error));

    int_store = std::numeric_limits<uint32>::max();
    EXPECT_EQ(std::numeric_limits<uint32>::max(), accessor->Get(&error));
  }
  {
    Error error;
    Uint32Accessor accessor(new ConstPropertyAccessor<uint32>(&int_store));
    EXPECT_EQ(int_store, accessor->Get(&error));

    uint32 expected_uint32 = 127;
    accessor->Set(expected_uint32, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(int_store, accessor->Get(&error));

    int_store = std::numeric_limits<uint32>::max();
    EXPECT_EQ(std::numeric_limits<uint32>::max(), accessor->Get(&error));
  }
  {
    Error error;
    Uint32Accessor accessor(new ConstPropertyAccessor<uint32>(&int_store));
    accessor->Clear(&error);
    ASSERT_FALSE(error.IsSuccess());
  }
  {
    Error error;
    Uint32Accessor accessor(new WriteOnlyPropertyAccessor<uint32>(&int_store));
    accessor->Get(&error);
    EXPECT_TRUE(error.IsFailure());
    EXPECT_EQ(Error::kPermissionDenied, error.type());
  }
  {
    Error error;
    uint32 expected_uint32 = 127;
    WriteOnlyPropertyAccessor<uint32> accessor(&int_store);
    accessor.Set(expected_uint32, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_uint32, *accessor.property_);
    EXPECT_EQ(uint32(), accessor.Get(&error));
    ASSERT_FALSE(error.IsSuccess());

    int_store = std::numeric_limits<uint32>::max();
    EXPECT_EQ(std::numeric_limits<uint32>::max(), *accessor.property_);
  }
  {
    Error error;
    uint32 orig_value = int_store = 0;
    WriteOnlyPropertyAccessor<uint32> accessor(&int_store);

    accessor.Set(127, &error);
    accessor.Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, *accessor.property_);
  }
}

TEST(PropertyAccessorTest, StringCorrectness) {
  string storage;
  {
    Error error;
    string orig_value = storage;
    StringAccessor accessor(new PropertyAccessor<string>(&storage));
    EXPECT_EQ(storage, accessor->Get(&error));

    string expected_string("what");
    accessor->Set(expected_string, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_string, accessor->Get(&error));

    accessor->Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, accessor->Get(&error));

    storage = "nooooo";
    EXPECT_EQ(storage, accessor->Get(&error));
  }
  {
    Error error;
    StringAccessor accessor(new ConstPropertyAccessor<string>(&storage));
    EXPECT_EQ(storage, accessor->Get(&error));

    string expected_string("what");
    accessor->Set(expected_string, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(storage, accessor->Get(&error));

    storage = "nooooo";
    EXPECT_EQ(storage, accessor->Get(&error));
  }
  {
    Error error;
    StringAccessor accessor(new ConstPropertyAccessor<string>(&storage));
    accessor->Clear(&error);
    ASSERT_FALSE(error.IsSuccess());
  }
  {
    Error error;
    StringAccessor accessor(new WriteOnlyPropertyAccessor<string>(&storage));
    accessor->Get(&error);
    EXPECT_TRUE(error.IsFailure());
    EXPECT_EQ(Error::kPermissionDenied, error.type());
  }
  {
    Error error;
    string expected_string = "what";
    WriteOnlyPropertyAccessor<string> accessor(&expected_string);
    accessor.Set(expected_string, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_string, *accessor.property_);
    EXPECT_EQ(string(), accessor.Get(&error));
    ASSERT_FALSE(error.IsSuccess());

    expected_string = "nooooo";
    EXPECT_EQ("nooooo", *accessor.property_);
  }
  {
    Error error;
    string orig_value = storage = "original value";
    WriteOnlyPropertyAccessor<string> accessor(&storage);
    accessor.Set("new value", &error);
    accessor.Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, *accessor.property_);
  }
}

class StringWrapper {
 public:
  string Get(Error */*error*/) {
    return value_;
  }
  void Set(const string &value, Error */*error*/) {
    value_ = value;
  }
  void Clear(Error */*error*/) {
    value_.clear();
  }

  string value_;
};

TEST(PropertyAccessorTest, CustomAccessorCorrectness) {
  StringWrapper wrapper;
  {
    // Custom accessor: read, write, clear, read-updated.
    Error error;
    const string orig_value = wrapper.value_ = "original value";
    CustomAccessor<StringWrapper, string> accessor(&wrapper,
                                                   &StringWrapper::Get,
                                                   &StringWrapper::Set);
    EXPECT_EQ(orig_value, accessor.Get(&error));
    EXPECT_TRUE(error.IsSuccess());

    const string expected_string = "new value";
    accessor.Set(expected_string, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_string, accessor.Get(&error));

    accessor.Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(orig_value, accessor.Get(&error));

    wrapper.value_ = "nooooo";
    EXPECT_EQ(wrapper.value_, accessor.Get(&error));
  }
  {
    // Custom read-only accessor: read, write, read-updated.
    Error error;
    CustomAccessor<StringWrapper, string> accessor(&wrapper,
                                                   &StringWrapper::Get,
                                                   NULL);
    EXPECT_EQ(wrapper.value_, accessor.Get(&error));

    const string expected_string = "what";
    accessor.Set(expected_string, &error);
    ASSERT_FALSE(error.IsSuccess());
    EXPECT_EQ(Error::kInvalidArguments, error.type());
    EXPECT_EQ(wrapper.value_, accessor.Get(&error));

    wrapper.value_ = "nooooo";
    EXPECT_EQ(wrapper.value_, accessor.Get(&error));
  }
  {
    // Custom read-only accessor: clear.
    Error error;
    CustomAccessor<StringWrapper, string> accessor(&wrapper,
                                                   &StringWrapper::Get,
                                                   NULL);
    accessor.Clear(&error);
    ASSERT_FALSE(error.IsSuccess());
  }
}

TEST(PropertyAccessorTest, CustomWriteOnlyAccessorWithDefault) {
  StringWrapper wrapper;
  {
    // Test reading.
    Error error;
    const string default_value = "default value";
    CustomWriteOnlyAccessor<StringWrapper, string> accessor(
        &wrapper, &StringWrapper::Set, NULL, &default_value);
    wrapper.value_ = "can't read this";
    EXPECT_EQ(string(), accessor.Get(&error));
    EXPECT_TRUE(error.IsFailure());
    EXPECT_EQ(Error::kPermissionDenied, error.type());
  }
  {
    // Test writing.
    Error error;
    const string default_value = "default value";
    const string expected_string = "what";
    CustomWriteOnlyAccessor<StringWrapper, string> accessor(
        &wrapper, &StringWrapper::Set, NULL, &default_value);
    accessor.Set(expected_string, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_string, wrapper.value_);
  }
  {
    // Test clearing.
    Error error;
    const string default_value = "default value";
    CustomWriteOnlyAccessor<StringWrapper, string> accessor(
        &wrapper, &StringWrapper::Set, NULL, &default_value);
    accessor.Set("new value", &error);
    EXPECT_EQ("new value", wrapper.value_);
    accessor.Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(default_value, wrapper.value_);
  }
}

TEST(PropertyAccessorTest, CustomWriteOnlyAccessorWithClear) {
  StringWrapper wrapper;
  {
    // Test reading.
    Error error;
    CustomWriteOnlyAccessor<StringWrapper, string> accessor(
        &wrapper, &StringWrapper::Set, &StringWrapper::Clear, NULL);
    wrapper.value_ = "can't read this";
    EXPECT_EQ(string(), accessor.Get(&error));
    EXPECT_TRUE(error.IsFailure());
    EXPECT_EQ(Error::kPermissionDenied, error.type());
  }
  {
    // Test writing.
    Error error;
    const string expected_string = "what";
    CustomWriteOnlyAccessor<StringWrapper, string> accessor(
        &wrapper, &StringWrapper::Set, &StringWrapper::Clear, NULL);
    accessor.Set(expected_string, &error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ(expected_string, wrapper.value_);
  }
  {
    // Test clearing.
    Error error;
    CustomWriteOnlyAccessor<StringWrapper, string> accessor(
        &wrapper, &StringWrapper::Set, &StringWrapper::Clear, NULL);
    accessor.Set("new value", &error);
    EXPECT_EQ("new value", wrapper.value_);
    accessor.Clear(&error);
    EXPECT_TRUE(error.IsSuccess());
    EXPECT_EQ("", wrapper.value_);
  }
}

}  // namespace shill

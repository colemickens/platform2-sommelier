// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/dbus_utils.h"

#include <limits>
#include <memory>
#include <vector>

#include <base/strings/string_number_conversions.h>
#include <dbus/dbus.h>
#include <dbus-c++/dbus.h>
#include <gtest/gtest.h>

using base::DictionaryValue;
using base::ListValue;
using base::Value;

class DBusUtilsTest : public testing::Test {
 protected:
  void ConvertDBusMessageToValues() {
    Value* value = NULL;
    ASSERT_TRUE(debugd::DBusMessageToValue(message_, &value));
    ASSERT_TRUE(value != NULL);
    raw_value_.reset(value);
    ASSERT_EQ(Value::TYPE_LIST, value->GetType());
    ListValue* list_values = NULL;
    ASSERT_TRUE(value->GetAsList(&list_values));
    values_.clear();
    for (size_t i = 0; i < list_values->GetSize(); i++) {
      Value* element = NULL;
      ASSERT_TRUE(list_values->Get(i, &element));
      values_.push_back(element);
    }
  }

  DBus::CallMessage message_;
  std::unique_ptr<Value> raw_value_;
  std::vector<Value*> values_;
};

TEST_F(DBusUtilsTest, DBusMessageToValueEmpty) {
  ConvertDBusMessageToValues();
  EXPECT_EQ(0, values_.size());
}

TEST_F(DBusUtilsTest, DBusMessageToValueOnePrimitive) {
  DBus::MessageIter iter = message_.writer();
  iter.append_bool(true);

  ConvertDBusMessageToValues();
  ASSERT_EQ(1, values_.size());

  Value* v = values_[0];
  EXPECT_EQ(Value::TYPE_BOOLEAN, v->GetType());
  bool b = false;
  EXPECT_TRUE(v->GetAsBoolean(&b));
  EXPECT_TRUE(b);
}

TEST_F(DBusUtilsTest, DBusMessageToValueMultiplePrimitives) {
  DBus::MessageIter iter = message_.writer();
  int v0_old = -3;
  int v1_old = 17;
  iter.append_int32(v0_old);
  iter.append_int32(v1_old);

  ConvertDBusMessageToValues();
  ASSERT_EQ(2, values_.size());

  int v0_new = 0;
  EXPECT_TRUE(values_[0]->GetAsInteger(&v0_new));
  EXPECT_EQ(v0_old, v0_new);

  int v1_new = 0;
  EXPECT_TRUE(values_[1]->GetAsInteger(&v1_new));
  EXPECT_EQ(v1_old, v1_new);
}

TEST_F(DBusUtilsTest, DBusMessageToValueArray) {
  DBus::MessageIter iter = message_.writer();
  DBus::MessageIter subiter = iter.new_array(DBUS_TYPE_INT32_AS_STRING);
  int v0_old = 7;
  int v1_old = -2;
  subiter.append_int32(v0_old);
  subiter.append_int32(v1_old);
  iter.close_container(subiter);

  ConvertDBusMessageToValues();
  ASSERT_EQ(1, values_.size());

  Value* v = values_[0];
  ASSERT_EQ(Value::TYPE_LIST, v->GetType());

  ListValue* lv = NULL;
  ASSERT_TRUE(v->GetAsList(&lv));
  ASSERT_TRUE(lv != NULL);
  ASSERT_EQ(2, lv->GetSize());

  Value* v0 = NULL;
  ASSERT_TRUE(lv->Get(0, &v0));
  ASSERT_TRUE(v0 != NULL);
  int v0_new;
  EXPECT_TRUE(v0->GetAsInteger(&v0_new));
  EXPECT_EQ(v0_old, v0_new);

  Value* v1 = NULL;
  ASSERT_TRUE(lv->Get(1, &v1));
  ASSERT_TRUE(v1 != NULL);
  int v1_new;
  EXPECT_TRUE(v1->GetAsInteger(&v1_new));
  EXPECT_EQ(v1_old, v1_new);
}

TEST_F(DBusUtilsTest, DBusMessageToValueDictionary) {
  char entry_type[5] = {
    DBUS_DICT_ENTRY_BEGIN_CHAR,
    DBUS_TYPE_STRING,
    DBUS_TYPE_INT32,
    DBUS_DICT_ENTRY_END_CHAR,
    '\0'
  };
  DBus::MessageIter iter = message_.writer();
  DBus::MessageIter subiter = iter.new_array(entry_type);
  DBus::MessageIter subsubiter = subiter.new_dict_entry();
  int foo_old = 17;
  subsubiter.append_string("foo");
  subsubiter.append_int32(foo_old);
  subiter.close_container(subsubiter);
  subsubiter = subiter.new_dict_entry();
  int bar_old = 12;
  subsubiter.append_string("bar");
  subsubiter.append_int32(bar_old);
  subiter.close_container(subsubiter);
  iter.close_container(subiter);

  ConvertDBusMessageToValues();
  ASSERT_EQ(1, values_.size());

  Value* v = values_[0];
  EXPECT_EQ(Value::TYPE_DICTIONARY, v->GetType());
  DictionaryValue* dv = NULL;
  ASSERT_TRUE(v->GetAsDictionary(&dv));
  int foo_new;
  ASSERT_TRUE(dv->GetInteger("foo", &foo_new));
  EXPECT_EQ(foo_old, foo_new);
  int bar_new;
  ASSERT_TRUE(dv->GetInteger("bar", &bar_new));
  EXPECT_EQ(bar_old, bar_new);
}

TEST_F(DBusUtilsTest, DBusMessageToValueIntTypes) {
  DBus::MessageIter iter = message_.writer();
  uint32_t uint32_max_old = std::numeric_limits<uint32_t>::max();
  iter.append_uint32(uint32_max_old);
  int64_t int64_min_old = std::numeric_limits<int64_t>::min();
  iter.append_int64(int64_min_old);
  uint64_t uint64_max_old = std::numeric_limits<uint64_t>::max();
  iter.append_uint64(uint64_max_old);

  ConvertDBusMessageToValues();
  ASSERT_EQ(3, values_.size());

  std::string uint32_max_new;
  std::string int64_min_new;
  std::string uint64_max_new;
  ASSERT_TRUE(values_[0]->GetAsString(&uint32_max_new));
  EXPECT_EQ(base::UintToString(uint32_max_old), uint32_max_new);
  ASSERT_TRUE(values_[1]->GetAsString(&int64_min_new));
  EXPECT_EQ(base::Int64ToString(int64_min_old), int64_min_new);
  ASSERT_TRUE(values_[2]->GetAsString(&uint64_max_new));
  EXPECT_EQ(base::Uint64ToString(uint64_max_old), uint64_max_new);
}

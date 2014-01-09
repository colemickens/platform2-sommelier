// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/test_helpers.h>
#include <chromeos/utility.h>

#include <limits>
#include <dbus/dbus.h>
#include <gtest/gtest.h>

#if BASE_VER >= 242728
#include <base/strings/string_number_conversions.h>
#else
#include <base/string_number_conversions.h>
#endif

#if BASE_VER >= 242728
using base::DictionaryValue;
using base::ListValue;
using base::Value;
#endif

static void MessageToValues(DBus::Message& m, std::vector<Value*>* result,
                            bool* success) {
  Value* v = NULL;
  *success = false;
  ASSERT_TRUE(chromeos::DBusMessageToValue(m, &v));
  ASSERT_TRUE(v != NULL);
  ASSERT_EQ(Value::TYPE_LIST, v->GetType());
  ListValue* lv = NULL;
  ASSERT_TRUE(v->GetAsList(&lv));
  std::vector<Value*> values;
  for (size_t i = 0; i < lv->GetSize(); i++) {
    Value* v0 = NULL;
    ASSERT_TRUE(lv->Get(i, &v0));
    values.push_back(v0);
  }
  *result = values;
  *success = true;
}

static void MessageToOneValue(DBus::Message& m, Value** result,
                               bool* success) {
  std::vector<Value*> values;
  MessageToValues(m, &values, success);
  ASSERT_EQ(1, values.size());
  *result = values[0];
  *success = true;
}

TEST(Utilities, DBusMessageToValueEmpty) {
  DBus::CallMessage m;
  std::vector<Value*> values;
  bool ok = false;
  MessageToValues(m, &values, &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(0, values.size());
}

TEST(Utilities, DBusMessageToValueOnePrimitive) {
  DBus::CallMessage m;
  DBus::MessageIter iter = m.writer();
  iter.append_bool(true);
  Value* v = NULL;
  bool ok = false;
  MessageToOneValue(m, &v, &ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(Value::TYPE_BOOLEAN, v->GetType());
  bool b;
  EXPECT_TRUE(v->GetAsBoolean(&b));
  EXPECT_TRUE(b);
}

TEST(Utilities, DBusMessageToValueMultiplePrimitives) {
  DBus::CallMessage m;
  DBus::MessageIter iter = m.writer();
  int v0_old = -3;
  int v1_old = 17;
  iter.append_int32(v0_old);
  iter.append_int32(v1_old);

  std::vector<Value*> values;
  bool ok = false;
  MessageToValues(m, &values, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ(2, values.size());

  int v0_new;
  EXPECT_TRUE(values[0]->GetAsInteger(&v0_new));
  EXPECT_EQ(v0_old, v0_new);

  int v1_new;
  EXPECT_TRUE(values[1]->GetAsInteger(&v1_new));
  EXPECT_EQ(v1_old, v1_new);
}

TEST(Utilities, DBusMessageToValueArray) {
  DBus::CallMessage m;
  DBus::MessageIter iter = m.writer();
  DBus::MessageIter subiter = iter.new_array(DBUS_TYPE_INT32_AS_STRING);
  int v0_old = 7;
  int v1_old = -2;
  subiter.append_int32(v0_old);
  subiter.append_int32(v1_old);
  iter.close_container(subiter);

  Value* v = NULL;
  bool ok = false;
  MessageToOneValue(m, &v, &ok);
  ASSERT_TRUE(ok);
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

TEST(Utilities, DBusMessageToValueDictionary) {
  char entry_type[5] = {
    DBUS_DICT_ENTRY_BEGIN_CHAR,
    DBUS_TYPE_STRING,
    DBUS_TYPE_INT32,
    DBUS_DICT_ENTRY_END_CHAR,
    '\0'
  };
  DBus::CallMessage m;
  DBus::MessageIter iter = m.writer();
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

  Value* v = NULL;
  bool ok = false;
  MessageToOneValue(m, &v, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ(Value::TYPE_DICTIONARY, v->GetType());

  // TODO(ellyjones): There should be a GetAsDictionary() method on Value.
  DictionaryValue* dv = static_cast<DictionaryValue*>(v);
  int foo_new;
  ASSERT_TRUE(dv->GetInteger("foo", &foo_new));
  EXPECT_EQ(foo_old, foo_new);
  int bar_new;
  ASSERT_TRUE(dv->GetInteger("bar", &bar_new));
  EXPECT_EQ(bar_old, bar_new);
}

TEST(Utilities, DBusMessageToValueIntTypes) {
  DBus::CallMessage m;
  DBus::MessageIter iter = m.writer();
  uint32_t uint32_max_old = std::numeric_limits<uint32_t>::max();
  iter.append_uint32(uint32_max_old);
  int64_t int64_min_old = std::numeric_limits<int64_t>::min();
  iter.append_int64(int64_min_old);
  uint64_t uint64_max_old = std::numeric_limits<uint64_t>::max();
  iter.append_uint64(uint64_max_old);
  std::vector<Value*> values;
  bool ok = false;
  MessageToValues(m, &values, &ok);
  ASSERT_TRUE(ok);
  ASSERT_EQ(3, values.size());
  std::string uint32_max_new;
  std::string int64_min_new;
  std::string uint64_max_new;
  ASSERT_TRUE(values[0]->GetAsString(&uint32_max_new));
  EXPECT_EQ(base::UintToString(uint32_max_old), uint32_max_new);
  ASSERT_TRUE(values[1]->GetAsString(&int64_min_new));
  EXPECT_EQ(base::Int64ToString(int64_min_old), int64_min_new);
  ASSERT_TRUE(values[2]->GetAsString(&uint64_max_new));
  EXPECT_EQ(base::Uint64ToString(uint64_max_old), uint64_max_new);
}

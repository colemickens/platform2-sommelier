// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/dbus_conversion.h"

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <base/guid.h>
#include <base/rand_util.h>
#include <base/values.h>
#include <chromeos/variant_dictionary.h>
#include <gtest/gtest.h>

#include "libweave/src/commands/object_schema.h"
#include "libweave/src/commands/unittest_utils.h"

namespace weave {

namespace {

using chromeos::Any;
using chromeos::VariantDictionary;
using unittests::CreateDictionaryValue;

chromeos::VariantDictionary ToDBus(const base::DictionaryValue& object) {
  return DictionaryToDBusVariantDictionary(object);
}

std::unique_ptr<base::DictionaryValue> FromDBus(
    const chromeos::VariantDictionary& object) {
  chromeos::ErrorPtr error;
  auto result = DictionaryFromDBusVariantDictionary(object, &error);
  EXPECT_TRUE(result || error);
  return result;
}

std::unique_ptr<base::Value> CreateRandomValue(int children);
std::unique_ptr<base::Value> CreateRandomValue(int children,
                                               base::Value::Type type);

const base::Value::Type kRandomTypes[] = {
    base::Value::TYPE_BOOLEAN,    base::Value::TYPE_INTEGER,
    base::Value::TYPE_DOUBLE,     base::Value::TYPE_STRING,
    base::Value::TYPE_DICTIONARY, base::Value::TYPE_LIST,
};

const base::Value::Type kRandomTypesWithChildren[] = {
    base::Value::TYPE_DICTIONARY, base::Value::TYPE_LIST,
};

base::Value::Type CreateRandomValueType(bool with_children) {
  if (with_children) {
    return kRandomTypesWithChildren[base::RandInt(
        0, arraysize(kRandomTypesWithChildren) - 1)];
  }
  return kRandomTypes[base::RandInt(0, arraysize(kRandomTypes) - 1)];
}

std::unique_ptr<base::DictionaryValue> CreateRandomDictionary(int children) {
  std::unique_ptr<base::DictionaryValue> result{new base::DictionaryValue};

  while (children > 0) {
    int sub_children = base::RandInt(1, children);
    children -= sub_children;
    result->Set(base::GenerateGUID(),
                CreateRandomValue(sub_children).release());
  }

  return result;
}

std::unique_ptr<base::ListValue> CreateRandomList(int children) {
  std::unique_ptr<base::ListValue> result{new base::ListValue};

  base::Value::Type type = CreateRandomValueType(children > 0);
  while (children > 0) {
    size_t max_children =
        (type != base::Value::TYPE_DICTIONARY && type != base::Value::TYPE_LIST)
            ? 1
            : children;
    size_t sub_children = base::RandInt(1, max_children);
    children -= sub_children;
    result->Append(CreateRandomValue(sub_children, type).release());
  }

  return result;
}

std::unique_ptr<base::Value> CreateRandomValue(int children,
                                               base::Value::Type type) {
  CHECK_GE(children, 1);
  switch (type) {
    case base::Value::TYPE_INTEGER:
      return std::unique_ptr<base::Value>{new base::FundamentalValue{
          base::RandInt(std::numeric_limits<int>::min(),
                        std::numeric_limits<int>::max())}};
    case base::Value::TYPE_DOUBLE:
      return std::unique_ptr<base::Value>{
          new base::FundamentalValue{base::RandDouble()}};
    case base::Value::TYPE_STRING:
      return std::unique_ptr<base::Value>{
          new base::StringValue{base::GenerateGUID()}};
    case base::Value::TYPE_DICTIONARY:
      CHECK_GE(children, 1);
      return CreateRandomDictionary(children - 1);
    case base::Value::TYPE_LIST:
      CHECK_GE(children, 1);
      return CreateRandomList(children - 1);
    default:
      return std::unique_ptr<base::Value>{
          new base::FundamentalValue{base::RandInt(0, 1) != 0}};
  }
}

std::unique_ptr<base::Value> CreateRandomValue(int children) {
  return CreateRandomValue(children, CreateRandomValueType(children > 0));
}

}  // namespace

TEST(DBusConversionTest, PropValueToDBusVariant) {
  IntPropType int_type;
  auto prop_value = int_type.CreateValue(5, nullptr);
  EXPECT_EQ(5, PropValueToDBusVariant(prop_value.get()).Get<int>());

  BooleanPropType bool_type;
  prop_value = bool_type.CreateValue(true, nullptr);
  EXPECT_TRUE(PropValueToDBusVariant(prop_value.get()).Get<bool>());

  DoublePropType dbl_type;
  prop_value = dbl_type.CreateValue(5.5, nullptr);
  EXPECT_DOUBLE_EQ(5.5, PropValueToDBusVariant(prop_value.get()).Get<double>());

  StringPropType str_type;
  prop_value = str_type.CreateValue(std::string{"foo"}, nullptr);
  EXPECT_EQ("foo", PropValueToDBusVariant(prop_value.get()).Get<std::string>());

  ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(
      CreateDictionaryValue(
          "{'properties':{'width':'integer','height':'integer'},"
          "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}")
          .get(),
      nullptr, nullptr));
  ValueMap obj{
      {"width", int_type.CreateValue(10, nullptr)},
      {"height", int_type.CreateValue(20, nullptr)},
  };
  prop_value = obj_type.CreateValue(obj, nullptr);
  VariantDictionary dict =
      PropValueToDBusVariant(prop_value.get()).Get<VariantDictionary>();
  EXPECT_EQ(20, dict["height"].Get<int>());
  EXPECT_EQ(10, dict["width"].Get<int>());

  ArrayPropType arr_type;
  arr_type.SetItemType(str_type.Clone());
  ValueVector arr;
  arr.push_back(str_type.CreateValue(std::string{"foo"}, nullptr));
  arr.push_back(str_type.CreateValue(std::string{"bar"}, nullptr));
  arr.push_back(str_type.CreateValue(std::string{"baz"}, nullptr));
  prop_value = arr_type.CreateValue(arr, nullptr);
  chromeos::Any any = PropValueToDBusVariant(prop_value.get());
  ASSERT_TRUE(any.IsTypeCompatible<std::vector<std::string>>());
  EXPECT_EQ((std::vector<std::string>{"foo", "bar", "baz"}),
            any.Get<std::vector<std::string>>());
}

TEST(DBusConversionTest, PropValueFromDBusVariant_Int) {
  IntPropType int_type;
  ASSERT_TRUE(int_type.FromJson(CreateDictionaryValue("{'enum':[1,2]}").get(),
                                nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&int_type, 1, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_EQ(1, prop_value->GetValueAsAny().Get<int>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&int_type, 5, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(DBusConversionTest, PropValueFromDBusVariant_Bool) {
  BooleanPropType bool_type;
  ASSERT_TRUE(bool_type.FromJson(CreateDictionaryValue("{'enum':[true]}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&bool_type, true, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_TRUE(prop_value->GetValueAsAny().Get<bool>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&bool_type, false, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(DBusConversionTest, PropValueFromDBusVariant_Double) {
  DoublePropType dbl_type;
  ASSERT_TRUE(dbl_type.FromJson(CreateDictionaryValue("{'maximum':2.0}").get(),
                                nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&dbl_type, 1.0, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_DOUBLE_EQ(1.0, prop_value->GetValueAsAny().Get<double>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&dbl_type, 10.0, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(DBusConversionTest, PropValueFromDBusVariant_String) {
  StringPropType str_type;
  ASSERT_TRUE(str_type.FromJson(CreateDictionaryValue("{'minLength': 4}").get(),
                                nullptr, nullptr));

  auto prop_value =
      PropValueFromDBusVariant(&str_type, std::string{"blah"}, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_EQ("blah", prop_value->GetValueAsAny().Get<std::string>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&str_type, std::string{"foo"}, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(DBusConversionTest, PropValueFromDBusVariant_Object) {
  ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(
      CreateDictionaryValue(
          "{'properties':{'width':'integer','height':'integer'},"
          "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}")
          .get(),
      nullptr, nullptr));

  VariantDictionary obj{
      {"width", 100}, {"height", 200},
  };
  auto prop_value = PropValueFromDBusVariant(&obj_type, obj, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  auto value = prop_value->GetValueAsAny().Get<ValueMap>();
  EXPECT_EQ(100, value["width"].get()->GetValueAsAny().Get<int>());
  EXPECT_EQ(200, value["height"].get()->GetValueAsAny().Get<int>());

  chromeos::ErrorPtr error;
  obj["height"] = 20;
  prop_value = PropValueFromDBusVariant(&obj_type, obj, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(DBusConversionTest, PropValueFromDBusVariant_Array) {
  ArrayPropType arr_type;
  IntPropType int_type;
  int_type.AddMinMaxConstraint(0, 100);
  arr_type.SetItemType(int_type.Clone());
  std::vector<int> data{0, 1, 1, 100};
  auto prop_value = PropValueFromDBusVariant(&arr_type, data, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  auto arr = prop_value->GetValueAsAny().Get<ValueVector>();
  ASSERT_EQ(4u, arr.size());
  EXPECT_EQ(0, arr[0]->GetInt()->GetValue());
  EXPECT_EQ(1, arr[1]->GetInt()->GetValue());
  EXPECT_EQ(1, arr[2]->GetInt()->GetValue());
  EXPECT_EQ(100, arr[3]->GetInt()->GetValue());

  chromeos::ErrorPtr error;
  data.push_back(-1);  // This value is out of bounds for |int_type|.
  prop_value = PropValueFromDBusVariant(&arr_type, data, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
}

TEST(DBusConversionTest, DictionaryToDBusVariantDictionary) {
  EXPECT_EQ((VariantDictionary{{"bool", true}}),
            ToDBus(*CreateDictionaryValue("{'bool': true}")));
  EXPECT_EQ((VariantDictionary{{"int", 5}}),
            ToDBus(*CreateDictionaryValue("{'int': 5}")));
  EXPECT_EQ((VariantDictionary{{"double", 6.7}}),
            ToDBus(*CreateDictionaryValue("{'double': 6.7}")));
  EXPECT_EQ((VariantDictionary{{"string", std::string{"abc"}}}),
            ToDBus(*CreateDictionaryValue("{'string': 'abc'}")));
  EXPECT_EQ((VariantDictionary{{"object", VariantDictionary{{"bool", true}}}}),
            ToDBus(*CreateDictionaryValue("{'object': {'bool': true}}")));
  EXPECT_EQ((VariantDictionary{{"emptyList", std::vector<Any>{}}}),
            ToDBus(*CreateDictionaryValue("{'emptyList': []}")));
  EXPECT_EQ((VariantDictionary{{"intList", std::vector<int>{5}}}),
            ToDBus(*CreateDictionaryValue("{'intList': [5]}")));
  EXPECT_EQ((VariantDictionary{
                {"intListList", std::vector<Any>{std::vector<int>{5},
                                                 std::vector<int>{6, 7}}}}),
            ToDBus(*CreateDictionaryValue("{'intListList': [[5], [6, 7]]}")));
  EXPECT_EQ((VariantDictionary{{"objList",
                                std::vector<VariantDictionary>{
                                    {{"string", std::string{"abc"}}}}}}),
            ToDBus(*CreateDictionaryValue("{'objList': [{'string': 'abc'}]}")));
}

TEST(DBusConversionTest, DictionaryFromDBusVariantDictionary) {
  EXPECT_JSON_EQ("{'bool': true}", *FromDBus({{"bool", true}}));
  EXPECT_JSON_EQ("{'int': 5}", *FromDBus({{"int", 5}}));
  EXPECT_JSON_EQ("{'double': 6.7}", *FromDBus({{"double", 6.7}}));
  EXPECT_JSON_EQ("{'string': 'abc'}",
                 *FromDBus({{"string", std::string{"abc"}}}));
  EXPECT_JSON_EQ("{'object': {'bool': true}}",
                 *FromDBus({{"object", VariantDictionary{{"bool", true}}}}));
  EXPECT_JSON_EQ("{'emptyList': []}",
                 *FromDBus({{"emptyList", std::vector<bool>{}}}));
  EXPECT_JSON_EQ("{'intList': [5]}",
                 *FromDBus({{"intList", std::vector<int>{5}}}));
  EXPECT_JSON_EQ(
      "{'intListList': [[5], [6, 7]]}",
      *FromDBus({{"intListList", std::vector<Any>{std::vector<int>{5},
                                                  std::vector<int>{6, 7}}}}));
  EXPECT_JSON_EQ(
      "{'objList': [{'string': 'abc'}]}",
      *FromDBus({{"objList", std::vector<VariantDictionary>{
                                 {{"string", std::string{"abc"}}}}}}));
  EXPECT_JSON_EQ("{'int': 5}", *FromDBus({{"int", Any{Any{5}}}}));
}

TEST(DBusConversionTest, DictionaryFromDBusVariantDictionary_Errors) {
  EXPECT_FALSE(FromDBus({{"cString", "abc"}}));
  EXPECT_FALSE(FromDBus({{"float", 1.0f}}));
  EXPECT_FALSE(FromDBus({{"listList", std::vector<std::vector<int>>{}}}));
  EXPECT_FALSE(FromDBus({{"any", Any{}}}));
  EXPECT_FALSE(FromDBus({{"null", nullptr}}));
}

TEST(DBusConversionTest, DBusRandomDictionaryConversion) {
  auto dict = CreateRandomDictionary(10000);
  auto varian_dict = ToDBus(*dict);
  auto dict_restored = FromDBus(varian_dict);
  EXPECT_PRED2(unittests::IsEqualValue, *dict, *dict_restored);
}

}  // namespace weave

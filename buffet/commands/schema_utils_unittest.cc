// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include <base/values.h>
#include <chromeos/variant_dictionary.h>
#include <gtest/gtest.h>

#include "buffet/commands/object_schema.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/commands/schema_utils.h"
#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateDictionaryValue;
using buffet::unittests::CreateValue;
using buffet::unittests::ValueToString;
using chromeos::VariantDictionary;

TEST(CommandSchemaUtils, TypedValueToJson_Scalar) {
  EXPECT_EQ("true",
            ValueToString(buffet::TypedValueToJson(true, nullptr).get()));
  EXPECT_EQ("false",
            ValueToString(buffet::TypedValueToJson(false, nullptr).get()));

  EXPECT_EQ("0", ValueToString(buffet::TypedValueToJson(0, nullptr).get()));
  EXPECT_EQ("-10", ValueToString(buffet::TypedValueToJson(-10, nullptr).get()));
  EXPECT_EQ("20", ValueToString(buffet::TypedValueToJson(20, nullptr).get()));

  EXPECT_EQ("0.0", ValueToString(buffet::TypedValueToJson(0.0, nullptr).get()));
  EXPECT_EQ("1.2", ValueToString(buffet::TypedValueToJson(1.2, nullptr).get()));

  EXPECT_EQ("'abc'",
            ValueToString(buffet::TypedValueToJson(std::string("abc"),
                                                   nullptr).get()));

  std::vector<bool> bool_array{true, false};
  EXPECT_EQ("[true,false]",
            ValueToString(buffet::TypedValueToJson(bool_array, nullptr).get()));

  std::vector<int> int_array{1, 2, 5};
  EXPECT_EQ("[1,2,5]",
            ValueToString(buffet::TypedValueToJson(int_array, nullptr).get()));

  std::vector<double> dbl_array{1.1, 2.2};
  EXPECT_EQ("[1.1,2.2]",
            ValueToString(buffet::TypedValueToJson(dbl_array, nullptr).get()));

  std::vector<std::string> str_array{"a", "bc"};
  EXPECT_EQ("['a','bc']",
            ValueToString(buffet::TypedValueToJson(str_array, nullptr).get()));
}

TEST(CommandSchemaUtils, TypedValueToJson_Object) {
  buffet::IntPropType int_type;
  buffet::native_types::Object object;

  object.insert(std::make_pair("width", int_type.CreateValue(640, nullptr)));
  object.insert(std::make_pair("height", int_type.CreateValue(480, nullptr)));
  EXPECT_EQ("{'height':480,'width':640}",
            ValueToString(buffet::TypedValueToJson(object, nullptr).get()));
}

TEST(CommandSchemaUtils, TypedValueFromJson_Bool) {
  bool value;

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("true").get(), nullptr,
                                         &value, nullptr));
  EXPECT_TRUE(value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("false").get(), nullptr,
                                         &value, nullptr));
  EXPECT_FALSE(value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("0").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Int) {
  int value;

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("0").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ(0, value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("23").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ(23, value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("-1234").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ(-1234, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Double) {
  double value;

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("0").get(), nullptr,
                                         &value, nullptr));
  EXPECT_DOUBLE_EQ(0.0, value);
  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("0.0").get(), nullptr,
                                         &value, nullptr));
  EXPECT_DOUBLE_EQ(0.0, value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("23").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ(23.0, value);
  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("23.1").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ(23.1, value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("-1.23E+02").get(),
                                         nullptr, &value, nullptr));
  EXPECT_EQ(-123.0, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_String) {
  std::string value;

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("''").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ("", value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("'23'").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ("23", value);

  EXPECT_TRUE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                         &value, nullptr));
  EXPECT_EQ("abc", value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("12").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Object) {
  buffet::native_types::Object value;
  buffet::ObjectSchema schema;

  auto age_prop = std::make_shared<buffet::IntPropType>();
  age_prop->AddMinMaxConstraint(0, 150);
  schema.AddProp("age", age_prop);

  auto name_prop = std::make_shared<buffet::StringPropType>();
  name_prop->AddLengthConstraint(1, 30);
  schema.AddProp("name", name_prop);

  EXPECT_TRUE(buffet::TypedValueFromJson(
      CreateValue("{'age':20,'name':'Bob'}").get(), &schema, &value, nullptr));
  buffet::native_types::Object value2;
  value2.insert(std::make_pair("age", age_prop->CreateValue(20, nullptr)));
  value2.insert(std::make_pair("name",
                               name_prop->CreateValue(std::string("Bob"),
                                                      nullptr)));
  EXPECT_EQ(value2, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, PropValueToDBusVariant) {
  buffet::IntPropType int_type;
  auto prop_value = int_type.CreateValue(5, nullptr);
  EXPECT_EQ(5, PropValueToDBusVariant(prop_value.get()).Get<int>());

  buffet::BooleanPropType bool_type;
  prop_value = bool_type.CreateValue(true, nullptr);
  EXPECT_TRUE(PropValueToDBusVariant(prop_value.get()).Get<bool>());

  buffet::DoublePropType dbl_type;
  prop_value = dbl_type.CreateValue(5.5, nullptr);
  EXPECT_DOUBLE_EQ(5.5, PropValueToDBusVariant(prop_value.get()).Get<double>());

  buffet::StringPropType str_type;
  prop_value = str_type.CreateValue(std::string{"foo"}, nullptr);
  EXPECT_EQ("foo", PropValueToDBusVariant(prop_value.get()).Get<std::string>());

  buffet::ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));
  buffet::native_types::Object obj{
    {"width", int_type.CreateValue(10, nullptr)},
    {"height", int_type.CreateValue(20, nullptr)},
  };
  prop_value = obj_type.CreateValue(obj, nullptr);
  VariantDictionary dict =
      PropValueToDBusVariant(prop_value.get()).Get<VariantDictionary>();
  EXPECT_EQ(20, dict["height"].Get<int>());
  EXPECT_EQ(10, dict["width"].Get<int>());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Int) {
  buffet::IntPropType int_type;
  ASSERT_TRUE(int_type.FromJson(CreateDictionaryValue("{'enum':[1,2]}").get(),
                                nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&int_type, 1, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_EQ(1, prop_value->GetValueAsAny().Get<int>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&int_type, 5, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Bool) {
  buffet::BooleanPropType bool_type;
  ASSERT_TRUE(bool_type.FromJson(CreateDictionaryValue("{'enum':[true]}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&bool_type, true, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_TRUE(prop_value->GetValueAsAny().Get<bool>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&bool_type, false, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Double) {
  buffet::DoublePropType dbl_type;
  ASSERT_TRUE(dbl_type.FromJson(CreateDictionaryValue("{'maximum':2.0}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&dbl_type, 1.0, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_DOUBLE_EQ(1.0, prop_value->GetValueAsAny().Get<double>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&dbl_type, 10.0, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_String) {
  buffet::StringPropType str_type;
  ASSERT_TRUE(str_type.FromJson(CreateDictionaryValue("{'minLength': 4}").get(),
                                 nullptr, nullptr));

  auto prop_value = PropValueFromDBusVariant(&str_type, std::string{"blah"},
                                             nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  EXPECT_EQ("blah", prop_value->GetValueAsAny().Get<std::string>());

  chromeos::ErrorPtr error;
  prop_value = PropValueFromDBusVariant(&str_type, std::string{"foo"}, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kOutOfRange, error->GetCode());
}

TEST(CommandSchemaUtils, PropValueFromDBusVariant_Object) {
  buffet::ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));

  VariantDictionary obj{
    {"width", 100},
    {"height", 200},
  };
  auto prop_value = PropValueFromDBusVariant(&obj_type, obj, nullptr);
  ASSERT_NE(nullptr, prop_value.get());
  auto value = prop_value->GetValueAsAny().Get<buffet::native_types::Object>();
  EXPECT_EQ(100, value["width"].get()->GetValueAsAny().Get<int>());
  EXPECT_EQ(200, value["height"].get()->GetValueAsAny().Get<int>());

  chromeos::ErrorPtr error;
  obj["height"] = 20;
  prop_value = PropValueFromDBusVariant(&obj_type, obj, &error);
  EXPECT_EQ(nullptr, prop_value.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kOutOfRange, error->GetCode());
}

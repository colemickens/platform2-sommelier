// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include <base/values.h>
#include <gtest/gtest.h>

#include "buffet/commands/object_schema.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/prop_values.h"
#include "buffet/commands/schema_utils.h"
#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateValue;
using buffet::unittests::ValueToString;

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

  object.insert(std::make_pair("width", int_type.CreateValue(640)));
  object.insert(std::make_pair("height", int_type.CreateValue(480)));
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

  buffet::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("0").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
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

  buffet::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
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

  buffet::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
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

  buffet::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("12").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
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
  value2.insert(std::make_pair("age", age_prop->CreateValue(20)));
  value2.insert(std::make_pair("name",
                               name_prop->CreateValue(std::string("Bob"))));
  EXPECT_EQ(value2, value);

  buffet::ErrorPtr error;
  EXPECT_FALSE(buffet::TypedValueFromJson(CreateValue("'abc'").get(), nullptr,
                                          &value, &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();
}

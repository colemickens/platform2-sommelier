// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/schema_utils.h"

#include <memory>
#include <string>
#include <vector>

#include <base/values.h>
#include <gtest/gtest.h>

#include "libweave/src/commands/object_schema.h"
#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/prop_values.h"
#include "libweave/src/commands/schema_constants.h"
#include "libweave/src/commands/unittest_utils.h"

namespace weave {

using unittests::CreateDictionaryValue;
using unittests::CreateValue;

TEST(CommandSchemaUtils, TypedValueToJson_Scalar) {
  EXPECT_JSON_EQ("true", *TypedValueToJson(true));
  EXPECT_JSON_EQ("false", *TypedValueToJson(false));

  EXPECT_JSON_EQ("0", *TypedValueToJson(0));
  EXPECT_JSON_EQ("-10", *TypedValueToJson(-10));
  EXPECT_JSON_EQ("20", *TypedValueToJson(20));

  EXPECT_JSON_EQ("0.0", *TypedValueToJson(0.0));
  EXPECT_JSON_EQ("1.2", *TypedValueToJson(1.2));

  EXPECT_JSON_EQ("'abc'", *TypedValueToJson(std::string("abc")));

  std::vector<bool> bool_array{true, false};
  EXPECT_JSON_EQ("[true,false]", *TypedValueToJson(bool_array));

  std::vector<int> int_array{1, 2, 5};
  EXPECT_JSON_EQ("[1,2,5]", *TypedValueToJson(int_array));

  std::vector<double> dbl_array{1.1, 2.2};
  EXPECT_JSON_EQ("[1.1,2.2]", *TypedValueToJson(dbl_array));

  std::vector<std::string> str_array{"a", "bc"};
  EXPECT_JSON_EQ("['a','bc']", *TypedValueToJson(str_array));
}

TEST(CommandSchemaUtils, TypedValueToJson_Object) {
  IntPropType int_type;
  ValueMap object;

  object.insert(std::make_pair(
      "width", int_type.CreateValue(base::FundamentalValue{640}, nullptr)));
  object.insert(std::make_pair(
      "height", int_type.CreateValue(base::FundamentalValue{480}, nullptr)));
  EXPECT_JSON_EQ("{'height':480,'width':640}", *TypedValueToJson(object));
}

TEST(CommandSchemaUtils, TypedValueToJson_Array) {
  IntPropType int_type;
  ValueVector arr;

  arr.push_back(int_type.CreateValue(base::FundamentalValue{640}, nullptr));
  arr.push_back(int_type.CreateValue(base::FundamentalValue{480}, nullptr));
  EXPECT_JSON_EQ("[640,480]", *TypedValueToJson(arr));
}

TEST(CommandSchemaUtils, TypedValueFromJson_Bool) {
  bool value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("true").get(), nullptr, &value, nullptr));
  EXPECT_TRUE(value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("false").get(), nullptr, &value, nullptr));
  EXPECT_FALSE(value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("0").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Int) {
  int value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("0").get(), nullptr, &value, nullptr));
  EXPECT_EQ(0, value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("23").get(), nullptr, &value, nullptr));
  EXPECT_EQ(23, value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("-1234").get(), nullptr, &value, nullptr));
  EXPECT_EQ(-1234, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Double) {
  double value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("0").get(), nullptr, &value, nullptr));
  EXPECT_DOUBLE_EQ(0.0, value);
  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("0.0").get(), nullptr, &value, nullptr));
  EXPECT_DOUBLE_EQ(0.0, value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("23").get(), nullptr, &value, nullptr));
  EXPECT_EQ(23.0, value);
  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("23.1").get(), nullptr, &value, nullptr));
  EXPECT_EQ(23.1, value);

  EXPECT_TRUE(TypedValueFromJson(CreateValue("-1.23E+02").get(), nullptr,
                                 &value, nullptr));
  EXPECT_EQ(-123.0, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_String) {
  std::string value;

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("''").get(), nullptr, &value, nullptr));
  EXPECT_EQ("", value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("'23'").get(), nullptr, &value, nullptr));
  EXPECT_EQ("23", value);

  EXPECT_TRUE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, nullptr));
  EXPECT_EQ("abc", value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("12").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Object) {
  ValueMap value;
  std::unique_ptr<ObjectSchema> schema{new ObjectSchema};

  IntPropType age_prop;
  age_prop.AddMinMaxConstraint(0, 150);
  schema->AddProp("age", age_prop.Clone());

  StringPropType name_prop;
  name_prop.AddLengthConstraint(1, 30);
  schema->AddProp("name", name_prop.Clone());

  ObjectPropType type;
  type.SetObjectSchema(std::move(schema));
  EXPECT_TRUE(TypedValueFromJson(CreateValue("{'age':20,'name':'Bob'}").get(),
                                 &type, &value, nullptr));
  ValueMap value2;
  value2.insert(std::make_pair(
      "age", age_prop.CreateValue(base::FundamentalValue{20}, nullptr)));
  value2.insert(std::make_pair(
      "name", name_prop.CreateValue(base::StringValue("Bob"), nullptr)));
  EXPECT_EQ(value2, value);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(
      TypedValueFromJson(CreateValue("'abc'").get(), nullptr, &value, &error));
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  error.reset();
}

TEST(CommandSchemaUtils, TypedValueFromJson_Array) {
  ValueVector arr;
  StringPropType str_type;
  str_type.AddLengthConstraint(3, 100);
  ArrayPropType type;
  type.SetItemType(str_type.Clone());

  EXPECT_TRUE(TypedValueFromJson(CreateValue("['foo', 'bar']").get(), &type,
                                 &arr, nullptr));
  ValueVector arr2;
  arr2.push_back(str_type.CreateValue(base::StringValue{"foo"}, nullptr));
  arr2.push_back(str_type.CreateValue(base::StringValue{"bar"}, nullptr));
  EXPECT_EQ(arr2, arr);

  chromeos::ErrorPtr error;
  EXPECT_FALSE(TypedValueFromJson(CreateValue("['baz', 'ab']").get(), &type,
                                  &arr, &error));
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
  error.reset();
}

}  // namespace weave

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/object_schema.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "buffet/commands/prop_constraints.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/commands/unittest_utils.h"

namespace buffet {

using unittests::CreateValue;
using unittests::CreateDictionaryValue;

namespace {

template <typename T>
std::vector<T> GetArrayValues(const native_types::Array& arr) {
  std::vector<T> values;
  values.reserve(arr.size());
  for (const auto& prop_value : arr) {
    values.push_back(prop_value->GetValueAsAny().Get<T>());
  }
  return values;
}

template <typename T>
std::vector<T> GetOneOfValues(const PropType* prop_type) {
  auto one_of = static_cast<const ConstraintOneOf*>(
      prop_type->GetConstraint(ConstraintType::OneOf));
  if (!one_of)
    return {};

  return GetArrayValues<T>(one_of->set_.value);
}

}  // anonymous namespace

TEST(CommandSchema, IntPropType_Empty) {
  IntPropType prop;
  EXPECT_TRUE(prop.GetConstraints().empty());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_EQ(nullptr, prop.GetDefaultValue());
}

TEST(CommandSchema, IntPropType_Types) {
  IntPropType prop;
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(&prop, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
  EXPECT_EQ(nullptr, prop.GetArray());
}

TEST(CommandSchema, IntPropType_ToJson) {
  IntPropType prop;
  EXPECT_JSON_EQ("'integer'", *prop.ToJson(false, nullptr));
  EXPECT_JSON_EQ("{'type':'integer'}", *prop.ToJson(true, nullptr));
  IntPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_JSON_EQ("{}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'minimum':3}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'minimum':3}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'maximum':-7}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'maximum':-7}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'minimum':0,'maximum':5}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'maximum':5,'minimum':0}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'enum':[1,2,3]}").get(), &prop,
                  nullptr);
  EXPECT_JSON_EQ("[1,2,3]", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'default':123}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'default':123}", *param2.ToJson(false, nullptr));
}

TEST(CommandSchema, IntPropType_FromJson) {
  IntPropType prop;
  prop.AddMinMaxConstraint(2, 8);
  IntPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_FALSE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(2, prop.GetMinValue());
  EXPECT_EQ(8, prop.GetMaxValue());
  prop.AddMinMaxConstraint(-2, 30);
  param2.FromJson(CreateDictionaryValue("{'minimum':7}").get(),
                  &prop, nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(7, param2.GetMinValue());
  EXPECT_EQ(30, param2.GetMaxValue());
  param2.FromJson(CreateDictionaryValue("{'maximum':17}").get(),
                  &prop, nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(-2, param2.GetMinValue());
  EXPECT_EQ(17, param2.GetMaxValue());

  ASSERT_TRUE(param2.FromJson(CreateDictionaryValue("{'default':3}").get(),
                              &prop, nullptr));
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  ASSERT_NE(nullptr, param2.GetDefaultValue());
  EXPECT_EQ(3, param2.GetDefaultValue()->GetInt()->GetValue());
}

TEST(CommandSchema, IntPropType_Validate) {
  IntPropType prop;
  prop.AddMinMaxConstraint(2, 4);
  chromeos::ErrorPtr error;
  EXPECT_FALSE(prop.ValidateValue(CreateValue("-1").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("0").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("1").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_TRUE(prop.ValidateValue(CreateValue("2").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(prop.ValidateValue(CreateValue("3").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(prop.ValidateValue(CreateValue("4").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_FALSE(prop.ValidateValue(CreateValue("5").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("true").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("3.0").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("'3'").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
}

TEST(CommandSchema, IntPropType_CreateValue) {
  IntPropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(2, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(2, val->GetValueAsAny().Get<int>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, BoolPropType_Empty) {
  BooleanPropType prop;
  EXPECT_TRUE(prop.GetConstraints().empty());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_EQ(nullptr, prop.GetDefaultValue());
}

TEST(CommandSchema, BoolPropType_Types) {
  BooleanPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(&prop, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
  EXPECT_EQ(nullptr, prop.GetArray());
}

TEST(CommandSchema, BoolPropType_ToJson) {
  BooleanPropType prop;
  EXPECT_JSON_EQ("'boolean'", *prop.ToJson(false, nullptr));
  EXPECT_JSON_EQ("{'type':'boolean'}", *prop.ToJson(true, nullptr));
  BooleanPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_JSON_EQ("{}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'enum':[true,false]}").get(), &prop,
                  nullptr);
  EXPECT_JSON_EQ("[true,false]", *param2.ToJson(false, nullptr));
  EXPECT_JSON_EQ("{'enum':[true,false],'type':'boolean'}",
                 *param2.ToJson(true, nullptr));
  param2.FromJson(CreateDictionaryValue("{'default':true}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'default':true}", *param2.ToJson(false, nullptr));
}

TEST(CommandSchema, BoolPropType_FromJson) {
  BooleanPropType prop;
  prop.FromJson(CreateDictionaryValue("{'enum':[true]}").get(), &prop,
                nullptr);
  BooleanPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_FALSE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(std::vector<bool>{true}, GetOneOfValues<bool>(&prop));

  BooleanPropType prop_base;
  BooleanPropType param3;
  ASSERT_TRUE(param3.FromJson(CreateDictionaryValue("{'default':false}").get(),
                              &prop_base, nullptr));
  EXPECT_TRUE(param3.HasOverriddenAttributes());
  ASSERT_NE(nullptr, param3.GetDefaultValue());
  EXPECT_FALSE(param3.GetDefaultValue()->GetBoolean()->GetValue());
}

TEST(CommandSchema, BoolPropType_Validate) {
  BooleanPropType prop;
  prop.FromJson(CreateDictionaryValue("{'enum':[true]}").get(), &prop,
                nullptr);
  chromeos::ErrorPtr error;
  EXPECT_FALSE(prop.ValidateValue(CreateValue("false").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_TRUE(prop.ValidateValue(CreateValue("true").get(), &error));
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("1").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("3.0").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("'3'").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
}

TEST(CommandSchema, BoolPropType_CreateValue) {
  BooleanPropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(true, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(val->GetValueAsAny().Get<bool>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, DoublePropType_Empty) {
  DoublePropType prop;
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::lowest(), prop.GetMinValue());
  EXPECT_DOUBLE_EQ((std::numeric_limits<double>::max)(), prop.GetMaxValue());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_EQ(nullptr, prop.GetDefaultValue());
}

TEST(CommandSchema, DoublePropType_Types) {
  DoublePropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(&prop, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
  EXPECT_EQ(nullptr, prop.GetArray());
}

TEST(CommandSchema, DoublePropType_ToJson) {
  DoublePropType prop;
  EXPECT_JSON_EQ("'number'", *prop.ToJson(false, nullptr));
  EXPECT_JSON_EQ("{'type':'number'}", *prop.ToJson(true, nullptr));
  DoublePropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_JSON_EQ("{}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'minimum':3}").get(), &prop,
                  nullptr);
  EXPECT_JSON_EQ("{'minimum':3.0}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'maximum':-7}").get(), &prop,
                  nullptr);
  EXPECT_JSON_EQ("{'maximum':-7.0}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'minimum':0,'maximum':5}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'maximum':5.0,'minimum':0.0}",
                 *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'default':12.3}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'default':12.3}", *param2.ToJson(false, nullptr));
}

TEST(CommandSchema, DoublePropType_FromJson) {
  DoublePropType prop;
  prop.AddMinMaxConstraint(2.5, 8.7);
  DoublePropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_FALSE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_DOUBLE_EQ(2.5, prop.GetMinValue());
  EXPECT_DOUBLE_EQ(8.7, prop.GetMaxValue());
  prop.AddMinMaxConstraint(-2.2, 30.4);
  param2.FromJson(CreateDictionaryValue("{'minimum':7}").get(), &prop,
                  nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_DOUBLE_EQ(7.0, param2.GetMinValue());
  EXPECT_DOUBLE_EQ(30.4, param2.GetMaxValue());
  param2.FromJson(CreateDictionaryValue("{'maximum':17.2}").get(), &prop,
                  nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_DOUBLE_EQ(-2.2, param2.GetMinValue());
  EXPECT_DOUBLE_EQ(17.2, param2.GetMaxValue());
  param2.FromJson(CreateDictionaryValue("{'minimum':0,'maximum':6.1}").get(),
                  &prop, nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_DOUBLE_EQ(0.0, param2.GetMinValue());
  EXPECT_DOUBLE_EQ(6.1, param2.GetMaxValue());

  ASSERT_TRUE(param2.FromJson(CreateDictionaryValue("{'default':-1.234}").get(),
                              &prop, nullptr));
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  ASSERT_NE(nullptr, param2.GetDefaultValue());
  EXPECT_DOUBLE_EQ(-1.234, param2.GetDefaultValue()->GetDouble()->GetValue());
}

TEST(CommandSchema, DoublePropType_Validate) {
  DoublePropType prop;
  prop.AddMinMaxConstraint(-1.2, 1.3);
  chromeos::ErrorPtr error;
  EXPECT_FALSE(prop.ValidateValue(CreateValue("-2").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("-1.3").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_TRUE(prop.ValidateValue(CreateValue("-1.2").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(prop.ValidateValue(CreateValue("0.0").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(prop.ValidateValue(CreateValue("1.3").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_FALSE(prop.ValidateValue(CreateValue("1.31").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("true").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("'0.0'").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
}

TEST(CommandSchema, DoublePropType_CreateValue) {
  DoublePropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(2.0, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_DOUBLE_EQ(2.0, val->GetValueAsAny().Get<double>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, StringPropType_Empty) {
  StringPropType prop;
  EXPECT_EQ(0, prop.GetMinLength());
  EXPECT_EQ((std::numeric_limits<int>::max)(), prop.GetMaxLength());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_EQ(nullptr, prop.GetDefaultValue());
}

TEST(CommandSchema, StringPropType_Types) {
  StringPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(&prop, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
  EXPECT_EQ(nullptr, prop.GetArray());
}

TEST(CommandSchema, StringPropType_ToJson) {
  StringPropType prop;
  EXPECT_JSON_EQ("'string'", *prop.ToJson(false, nullptr));
  EXPECT_JSON_EQ("{'type':'string'}", *prop.ToJson(true, nullptr));
  StringPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_JSON_EQ("{}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'minLength':3}").get(), &prop,
                  nullptr);
  EXPECT_JSON_EQ("{'minLength':3}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'maxLength':7}").get(), &prop,
                  nullptr);
  EXPECT_JSON_EQ("{'maxLength':7}", *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'minLength':0,'maxLength':5}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'maxLength':5,'minLength':0}",
                 *param2.ToJson(false, nullptr));
  param2.FromJson(CreateDictionaryValue("{'default':'abcd'}").get(),
                  &prop, nullptr);
  EXPECT_JSON_EQ("{'default':'abcd'}", *param2.ToJson(false, nullptr));
}

TEST(CommandSchema, StringPropType_FromJson) {
  StringPropType prop;
  prop.AddLengthConstraint(2, 8);
  StringPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_FALSE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(2, prop.GetMinLength());
  EXPECT_EQ(8, prop.GetMaxLength());
  prop.AddLengthConstraint(3, 5);
  param2.FromJson(CreateDictionaryValue("{'minLength':4}").get(), &prop,
                  nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(4, param2.GetMinLength());
  EXPECT_EQ(5, param2.GetMaxLength());
  param2.FromJson(CreateDictionaryValue("{'maxLength':8}").get(), &prop,
                  nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(3, param2.GetMinLength());
  EXPECT_EQ(8, param2.GetMaxLength());
  param2.FromJson(CreateDictionaryValue(
      "{'minLength':1,'maxLength':7}").get(), &prop, nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(1, param2.GetMinLength());
  EXPECT_EQ(7, param2.GetMaxLength());

  ASSERT_TRUE(param2.FromJson(CreateDictionaryValue("{'default':'foo'}").get(),
                              &prop, nullptr));
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  ASSERT_NE(nullptr, param2.GetDefaultValue());
  EXPECT_EQ("foo", param2.GetDefaultValue()->GetString()->GetValue());
}

TEST(CommandSchema, StringPropType_Validate) {
  StringPropType prop;
  prop.AddLengthConstraint(1, 3);
  chromeos::ErrorPtr error;
  EXPECT_FALSE(prop.ValidateValue(CreateValue("''").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  prop.AddLengthConstraint(2, 3);
  EXPECT_FALSE(prop.ValidateValue(CreateValue("''").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_FALSE(prop.ValidateValue(CreateValue("'a'").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
  EXPECT_TRUE(prop.ValidateValue(CreateValue("'ab'").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(prop.ValidateValue(CreateValue("'abc'").get(), &error));
  EXPECT_EQ(nullptr, error.get());
  EXPECT_FALSE(prop.ValidateValue(CreateValue("'abcd'").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();

  prop.FromJson(CreateDictionaryValue("{'enum':['abc','def','xyz!!']}").get(),
                nullptr, &error);
  EXPECT_TRUE(prop.ValidateValue(CreateValue("'abc'").get(), &error));
  EXPECT_TRUE(prop.ValidateValue(CreateValue("'def'").get(), &error));
  EXPECT_TRUE(prop.ValidateValue(CreateValue("'xyz!!'").get(), &error));
  EXPECT_FALSE(prop.ValidateValue(CreateValue("'xyz'").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
}

TEST(CommandSchema, StringPropType_CreateValue) {
  StringPropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(std::string{"blah"}, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ("blah", val->GetValueAsAny().Get<std::string>());

  val = prop.CreateValue(4, &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, ObjectPropType_Empty) {
  ObjectPropType prop;
  EXPECT_TRUE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_EQ(nullptr, prop.GetDefaultValue());
}

TEST(CommandSchema, ObjectPropType_Types) {
  ObjectPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(&prop, prop.GetObject());
  EXPECT_EQ(nullptr, prop.GetArray());
}

TEST(CommandSchema, ObjectPropType_ToJson) {
  ObjectPropType prop;
  EXPECT_JSON_EQ("{'additionalProperties':false,'properties':{}}",
                 *prop.ToJson(false, nullptr));
  EXPECT_JSON_EQ(
      "{'additionalProperties':false,'properties':{},'type':'object'}",
      *prop.ToJson(true, nullptr));
  EXPECT_FALSE(prop.IsBasedOnSchema());
  ObjectPropType prop2;
  prop2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_JSON_EQ("{}", *prop2.ToJson(false, nullptr));
  EXPECT_TRUE(prop2.IsBasedOnSchema());

  auto schema = ObjectSchema::Create();
  schema->AddProp("expires", PropType::Create(ValueType::Int));
  auto pw = PropType::Create(ValueType::String);
  pw->GetString()->AddLengthConstraint(6, 100);
  schema->AddProp("password", std::move(pw));
  prop2.SetObjectSchema(std::move(schema));
  auto expected = R"({
    'additionalProperties': false,
    'properties': {
      'expires': 'integer',
      'password': {
        'maxLength': 100,
        'minLength': 6
      }
    }
  })";
  EXPECT_JSON_EQ(expected, *prop2.ToJson(false, nullptr));

  expected = R"({
    'additionalProperties': false,
    'properties': {
      'expires': {
        'type': 'integer'
      },
      'password': {
        'maxLength': 100,
        'minLength': 6,
        'type': 'string'
      }
    },
    'type': 'object'
  })";
  EXPECT_JSON_EQ(expected, *prop2.ToJson(true, nullptr));

  ObjectPropType prop3;
  ASSERT_TRUE(prop3.FromJson(CreateDictionaryValue(
      "{'default':{'expires':3,'password':'abracadabra'}}").get(), &prop2,
      nullptr));
  expected = R"({
    'default': {
      'expires': 3,
      'password': 'abracadabra'
    }
  })";
  EXPECT_JSON_EQ(expected, *prop3.ToJson(false, nullptr));

  expected = R"({
    'additionalProperties': false,
    'default': {
      'expires': 3,
      'password': 'abracadabra'
    },
    'properties': {
      'expires': {
        'type': 'integer'
      },
      'password': {
        'maxLength': 100,
        'minLength': 6,
        'type': 'string'
      }
    },
    'type': 'object'
  })";
  EXPECT_JSON_EQ(expected, *prop3.ToJson(true, nullptr));

  ObjectPropType prop4;
  ASSERT_TRUE(prop4.FromJson(CreateDictionaryValue(
      "{'additionalProperties':true,"
      "'default':{'expires':3,'password':'abracadabra'}}").get(), &prop2,
      nullptr));
  expected = R"({
    'additionalProperties': true,
    'default': {
      'expires': 3,
      'password': 'abracadabra'
    },
    'properties': {
      'expires': 'integer',
      'password': {
        'maxLength': 100,
        'minLength': 6
      }
    }
  })";
  EXPECT_JSON_EQ(expected, *prop4.ToJson(false, nullptr));

  expected = R"({
    'additionalProperties': true,
    'default': {
      'expires': 3,
      'password': 'abracadabra'
    },
    'properties': {
      'expires': {
        'type': 'integer'
      },
      'password': {
        'maxLength': 100,
        'minLength': 6,
        'type': 'string'
      }
    },
    'type': 'object'
  })";
  EXPECT_JSON_EQ(expected, *prop4.ToJson(true, nullptr));
}

TEST(CommandSchema, ObjectPropType_FromJson) {
  ObjectPropType base_prop;
  EXPECT_TRUE(base_prop.FromJson(CreateDictionaryValue(
      "{'properties':{'name':'string','age':'integer'}}").get(), nullptr,
      nullptr));
  auto schema = base_prop.GetObject()->GetObjectSchemaPtr();
  const PropType* prop = schema->GetProp("name");
  EXPECT_EQ(ValueType::String, prop->GetType());
  prop = schema->GetProp("age");
  EXPECT_EQ(ValueType::Int, prop->GetType());

  ObjectPropType prop2;
  ASSERT_TRUE(prop2.FromJson(CreateDictionaryValue(
      "{'properties':{'name':'string','age':'integer'},"
      "'default':{'name':'Bob','age':33}}").get(), nullptr, nullptr));
  ASSERT_NE(nullptr, prop2.GetDefaultValue());
  const ObjectValue* defval = prop2.GetDefaultValue()->GetObject();
  ASSERT_NE(nullptr, defval);
  native_types::Object objval = defval->GetValue();
  EXPECT_EQ("Bob", objval["name"]->GetString()->GetValue());
  EXPECT_EQ(33, objval["age"]->GetInt()->GetValue());
}

TEST(CommandSchema, ObjectPropType_Validate) {
  ObjectPropType prop;
  prop.FromJson(CreateDictionaryValue(
      "{'properties':{'expires':'integer',"
      "'password':{'maxLength':100,'minLength':6}}}").get(), nullptr,
      nullptr);
  chromeos::ErrorPtr error;
  EXPECT_TRUE(prop.ValidateValue(CreateValue(
      "{'expires':10,'password':'abcdef'}").get(), &error));
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue(
      "{'expires':10}").get(), &error));
  EXPECT_EQ("parameter_missing", error->GetCode());
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue(
      "{'password':'abcdef'}").get(), &error));
  EXPECT_EQ("parameter_missing", error->GetCode());
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue(
      "{'expires':10,'password':'abcde'}").get(), &error));
  EXPECT_EQ("out_of_range", error->GetFirstError()->GetCode());
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue("2").get(), &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue(
      "{'expires':10,'password':'abcdef','retry':true}").get(), &error));
  EXPECT_EQ("unexpected_parameter", error->GetCode());
  error.reset();
}

TEST(CommandSchema, ObjectPropType_Validate_Enum) {
  ObjectPropType prop;
  EXPECT_TRUE(prop.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));
  chromeos::ErrorPtr error;
  EXPECT_TRUE(prop.ValidateValue(CreateValue(
      "{'height':20,'width':10}").get(), &error));
  error.reset();

  EXPECT_TRUE(prop.ValidateValue(CreateValue(
      "{'height':200,'width':100}").get(), &error));
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue(
      "{'height':12,'width':10}").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
}

TEST(CommandSchema, ObjectPropType_CreateValue) {
  ObjectPropType prop;
  IntPropType int_type;
  ASSERT_TRUE(prop.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));
  native_types::Object obj{
      {"width", int_type.CreateValue(10, nullptr)},
      {"height", int_type.CreateValue(20, nullptr)},
  };

  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(obj, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(obj, val->GetValueAsAny().Get<native_types::Object>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, ArrayPropType_Empty) {
  ArrayPropType prop;
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_EQ(nullptr, prop.GetDefaultValue());
  EXPECT_EQ(nullptr, prop.GetItemTypePtr());
  prop.SetItemType(PropType::Create(ValueType::Int));
  EXPECT_TRUE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
  EXPECT_NE(nullptr, prop.GetItemTypePtr());
}

TEST(CommandSchema, ArrayPropType_Types) {
  ArrayPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
  EXPECT_EQ(&prop, prop.GetArray());
}

TEST(CommandSchema, ArrayPropType_ToJson) {
  ArrayPropType prop;
  prop.SetItemType(PropType::Create(ValueType::Int));
  EXPECT_JSON_EQ("{'items':'integer'}", *prop.ToJson(false, nullptr));
  EXPECT_JSON_EQ("{'items':{'type':'integer'},'type':'array'}",
                 *prop.ToJson(true, nullptr));
  EXPECT_FALSE(prop.IsBasedOnSchema());
  ArrayPropType prop2;
  prop2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_JSON_EQ("{}", *prop2.ToJson(false, nullptr));
  EXPECT_TRUE(prop2.IsBasedOnSchema());
  prop2.FromJson(CreateDictionaryValue("{'default':[1,2,3]}").get(),
                 &prop, nullptr);
  EXPECT_JSON_EQ("{'default':[1,2,3]}", *prop2.ToJson(false, nullptr));
  EXPECT_JSON_EQ(
      "{'default':[1,2,3],'items':{'type':'integer'},'type':'array'}",
      *prop2.ToJson(true, nullptr));
}

TEST(CommandSchema, ArrayPropType_FromJson) {
  ArrayPropType prop;
  EXPECT_TRUE(prop.FromJson(
      CreateDictionaryValue("{'items':'integer'}").get(), nullptr, nullptr));
  EXPECT_EQ(ValueType::Int, prop.GetItemTypePtr()->GetType());

  ArrayPropType prop2;
  ASSERT_TRUE(prop2.FromJson(CreateDictionaryValue(
    "{'items':'string','default':['foo', 'bar', 'baz']}").get(), nullptr,
      nullptr));
  ASSERT_NE(nullptr, prop2.GetDefaultValue());
  const ArrayValue* defval = prop2.GetDefaultValue()->GetArray();
  ASSERT_NE(nullptr, defval);
  EXPECT_EQ((std::vector<std::string>{"foo", "bar", "baz"}),
            GetArrayValues<std::string>(defval->GetValue()));
}

TEST(CommandSchema, ArrayPropType_Validate) {
  ArrayPropType prop;
  prop.FromJson(CreateDictionaryValue(
      "{'items':{'minimum':2.3, 'maximum':10.5}}").get(), nullptr,
      nullptr);

  chromeos::ErrorPtr error;
  EXPECT_TRUE(prop.ValidateValue(CreateValue("[3,4,10.5]").get(), &error));
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue("[2]").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  EXPECT_EQ("Value 2 is out of range. It must not be less than 2.3",
            error->GetMessage());
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue("[4, 5, 20]").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  EXPECT_EQ("Value 20 is out of range. It must not be greater than 10.5",
            error->GetMessage());
  error.reset();
}

TEST(CommandSchema, ArrayPropType_Validate_Enum) {
  ArrayPropType prop;
  prop.FromJson(CreateDictionaryValue(
      "{'items':'integer', 'enum':[[1], [2,3], [4,5,6]]}").get(), nullptr,
      nullptr);

  chromeos::ErrorPtr error;
  EXPECT_TRUE(prop.ValidateValue(CreateValue("[2,3]").get(), &error));
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue("[2]").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  EXPECT_EQ("Value [2] is invalid. Expected one of [[1],[2,3],[4,5,6]]",
            error->GetMessage());
  error.reset();

  EXPECT_FALSE(prop.ValidateValue(CreateValue("[2,3,4]").get(), &error));
  EXPECT_EQ("out_of_range", error->GetCode());
  error.reset();
}

TEST(CommandSchema, ArrayPropType_CreateValue) {
  ArrayPropType prop;
  ASSERT_TRUE(prop.FromJson(CreateDictionaryValue(
      "{'items':{'properties':{'width':'integer','height':'integer'}}}").get(),
      nullptr, nullptr));

  chromeos::ErrorPtr error;
  native_types::Array arr;

  auto val = prop.CreateValue(arr, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(arr, val->GetValueAsAny().Get<native_types::Array>());
  EXPECT_JSON_EQ("[]", *val->ToJson(nullptr));

  IntPropType int_type;
  ObjectPropType obj_type;
  ASSERT_TRUE(obj_type.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'}}").get(),
      nullptr, nullptr));
  arr.push_back(obj_type.CreateValue(
      native_types::Object{
          {"width", int_type.CreateValue(10, nullptr)},
          {"height", int_type.CreateValue(20, nullptr)},
      },
      nullptr));
  arr.push_back(obj_type.CreateValue(
      native_types::Object{
          {"width", int_type.CreateValue(17, nullptr)},
          {"height", int_type.CreateValue(18, nullptr)},
      },
      nullptr));

  val = prop.CreateValue(arr, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(arr, val->GetValueAsAny().Get<native_types::Array>());
  EXPECT_JSON_EQ("[{'height':20,'width':10},{'height':18,'width':17}]",
                 *val->ToJson(nullptr));

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
}

TEST(CommandSchema, ArrayPropType_NestedArrays_NotSupported) {
  ArrayPropType prop;
  chromeos::ErrorPtr error;
  EXPECT_FALSE(prop.FromJson(CreateDictionaryValue(
      "{'items':{'items':'integer'}}").get(), nullptr, &error));
  EXPECT_EQ(errors::commands::kInvalidObjectSchema, error->GetCode());
  error.reset();
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, ObjectSchema_FromJson_Shorthand_TypeName) {
  ObjectSchema schema;
  const char* schema_str = "{"
      "'param1':'integer',"
      "'param2':'number',"
      "'param3':'string'"
      "}";
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                              nullptr));
  EXPECT_EQ(ValueType::Int, schema.GetProp("param1")->GetType());
  EXPECT_EQ(ValueType::Double, schema.GetProp("param2")->GetType());
  EXPECT_EQ(ValueType::String, schema.GetProp("param3")->GetType());
  EXPECT_EQ("integer", schema.GetProp("param1")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param2")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param3")->GetTypeAsString());
  EXPECT_EQ(nullptr, schema.GetProp("param4"));

  int min_int = (std::numeric_limits<int>::min)();
  int max_int = (std::numeric_limits<int>::max)();
  double min_dbl = (std::numeric_limits<double>::lowest)();
  double max_dbl = (std::numeric_limits<double>::max)();
  EXPECT_EQ(min_int, schema.GetProp("param1")->GetInt()->GetMinValue());
  EXPECT_EQ(max_int, schema.GetProp("param1")->GetInt()->GetMaxValue());
  EXPECT_EQ(min_dbl, schema.GetProp("param2")->GetDouble()->GetMinValue());
  EXPECT_EQ(max_dbl, schema.GetProp("param2")->GetDouble()->GetMaxValue());
  EXPECT_EQ(0, schema.GetProp("param3")->GetString()->GetMinLength());
  EXPECT_EQ(max_int, schema.GetProp("param3")->GetString()->GetMaxLength());
}

TEST(CommandSchema, ObjectSchema_FromJson_Full_TypeName) {
  ObjectSchema schema;
  const char* schema_str = "{"
      "'param1':{'type':'integer'},"
      "'param2':{'type':'number'},"
      "'param3':{'type':'string'},"
      "'param4':{'type':'array', 'items':'integer'},"
      "'param5':{'type':'object', 'properties':{'p1':'integer'}}"
      "}";
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                              nullptr));
  EXPECT_EQ(ValueType::Int, schema.GetProp("param1")->GetType());
  EXPECT_EQ(ValueType::Double, schema.GetProp("param2")->GetType());
  EXPECT_EQ(ValueType::String, schema.GetProp("param3")->GetType());
  EXPECT_EQ(ValueType::Array, schema.GetProp("param4")->GetType());
  EXPECT_EQ(ValueType::Object, schema.GetProp("param5")->GetType());
  EXPECT_EQ("integer", schema.GetProp("param1")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param2")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param3")->GetTypeAsString());
  EXPECT_EQ("array", schema.GetProp("param4")->GetTypeAsString());
  EXPECT_EQ("object", schema.GetProp("param5")->GetTypeAsString());
  EXPECT_EQ(nullptr, schema.GetProp("param77"));

  int min_int = (std::numeric_limits<int>::min)();
  int max_int = (std::numeric_limits<int>::max)();
  double min_dbl = (std::numeric_limits<double>::lowest)();
  double max_dbl = (std::numeric_limits<double>::max)();
  EXPECT_EQ(min_int, schema.GetProp("param1")->GetInt()->GetMinValue());
  EXPECT_EQ(max_int, schema.GetProp("param1")->GetInt()->GetMaxValue());
  EXPECT_EQ(min_dbl, schema.GetProp("param2")->GetDouble()->GetMinValue());
  EXPECT_EQ(max_dbl, schema.GetProp("param2")->GetDouble()->GetMaxValue());
  EXPECT_EQ(0, schema.GetProp("param3")->GetString()->GetMinLength());
  EXPECT_EQ(max_int, schema.GetProp("param3")->GetString()->GetMaxLength());
}

TEST(CommandSchema, ObjectSchema_FromJson_Shorthand_TypeDeduction_Scalar) {
  ObjectSchema schema;
  const char* schema_str = "{"
      "'param1' :{'minimum':2},"
      "'param2' :{'maximum':10},"
      "'param3' :{'maximum':8, 'minimum':2},"
      "'param4' :{'minimum':2.1},"
      "'param5' :{'maximum':10.1},"
      "'param6' :{'maximum':8.1, 'minimum':3.1},"
      "'param7' :{'maximum':8, 'minimum':3.1},"
      "'param8' :{'maximum':8.1, 'minimum':3},"
      "'param9' :{'minLength':2},"
      "'param10':{'maxLength':10},"
      "'param11':{'maxLength':8, 'minLength':3},"
      "'param12':{'default':12},"
      "'param13':{'default':13.5},"
      "'param14':{'default':true},"
      "'param15':{'default':false},"
      "'param16':{'default':'foobar'},"
      "'param17':{'default':[1,2,3]},"
      "'param18':{'items':'number', 'default':[]}"
      "}";
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                              nullptr));
  EXPECT_EQ("integer", schema.GetProp("param1")->GetTypeAsString());
  EXPECT_EQ("integer", schema.GetProp("param2")->GetTypeAsString());
  EXPECT_EQ("integer", schema.GetProp("param3")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param4")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param5")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param6")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param7")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param8")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param9")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param10")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param11")->GetTypeAsString());
  EXPECT_EQ("integer", schema.GetProp("param12")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param13")->GetTypeAsString());
  EXPECT_EQ("boolean", schema.GetProp("param14")->GetTypeAsString());
  EXPECT_EQ("boolean", schema.GetProp("param15")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param16")->GetTypeAsString());
  EXPECT_EQ("array", schema.GetProp("param17")->GetTypeAsString());
  auto prop17 = schema.GetProp("param17");
  EXPECT_EQ("integer",
            prop17->GetArray()->GetItemTypePtr()->GetTypeAsString());
  EXPECT_EQ("array", schema.GetProp("param18")->GetTypeAsString());
  auto prop18 = schema.GetProp("param18");
  EXPECT_EQ("number",
            prop18->GetArray()->GetItemTypePtr()->GetTypeAsString());

  int min_int = (std::numeric_limits<int>::min)();
  int max_int = (std::numeric_limits<int>::max)();
  double min_dbl = (std::numeric_limits<double>::lowest)();
  double max_dbl = (std::numeric_limits<double>::max)();
  EXPECT_EQ(2, schema.GetProp("param1")->GetInt()->GetMinValue());
  EXPECT_EQ(max_int, schema.GetProp("param1")->GetInt()->GetMaxValue());
  EXPECT_EQ(min_int, schema.GetProp("param2")->GetInt()->GetMinValue());
  EXPECT_EQ(10, schema.GetProp("param2")->GetInt()->GetMaxValue());
  EXPECT_EQ(2, schema.GetProp("param3")->GetInt()->GetMinValue());
  EXPECT_EQ(8, schema.GetProp("param3")->GetInt()->GetMaxValue());
  EXPECT_DOUBLE_EQ(2.1, schema.GetProp("param4")->GetDouble()->GetMinValue());
  EXPECT_DOUBLE_EQ(max_dbl,
                   schema.GetProp("param4")->GetDouble()->GetMaxValue());
  EXPECT_DOUBLE_EQ(min_dbl,
                   schema.GetProp("param5")->GetDouble()->GetMinValue());
  EXPECT_DOUBLE_EQ(10.1, schema.GetProp("param5")->GetDouble()->GetMaxValue());
  EXPECT_DOUBLE_EQ(3.1, schema.GetProp("param6")->GetDouble()->GetMinValue());
  EXPECT_DOUBLE_EQ(8.1, schema.GetProp("param6")->GetDouble()->GetMaxValue());
  EXPECT_DOUBLE_EQ(3.1, schema.GetProp("param7")->GetDouble()->GetMinValue());
  EXPECT_DOUBLE_EQ(8.0, schema.GetProp("param7")->GetDouble()->GetMaxValue());
  EXPECT_DOUBLE_EQ(3.0, schema.GetProp("param8")->GetDouble()->GetMinValue());
  EXPECT_DOUBLE_EQ(8.1, schema.GetProp("param8")->GetDouble()->GetMaxValue());
  EXPECT_EQ(2, schema.GetProp("param9")->GetString()->GetMinLength());
  EXPECT_EQ(max_int, schema.GetProp("param9")->GetString()->GetMaxLength());
  EXPECT_EQ(0, schema.GetProp("param10")->GetString()->GetMinLength());
  EXPECT_EQ(10, schema.GetProp("param10")->GetString()->GetMaxLength());
  EXPECT_EQ(3, schema.GetProp("param11")->GetString()->GetMinLength());
  EXPECT_EQ(8, schema.GetProp("param11")->GetString()->GetMaxLength());
  const PropValue* val = schema.GetProp("param12")->GetDefaultValue();
  EXPECT_EQ(12, val->GetInt()->GetValue());
  val = schema.GetProp("param13")->GetDefaultValue();
  EXPECT_DOUBLE_EQ(13.5, val->GetDouble()->GetValue());
  val = schema.GetProp("param14")->GetDefaultValue();
  EXPECT_TRUE(val->GetBoolean()->GetValue());
  val = schema.GetProp("param15")->GetDefaultValue();
  EXPECT_FALSE(val->GetBoolean()->GetValue());
  val = schema.GetProp("param16")->GetDefaultValue();
  EXPECT_EQ("foobar", val->GetString()->GetValue());
  val = schema.GetProp("param17")->GetDefaultValue();
  EXPECT_EQ((std::vector<int>{1, 2, 3}),
            GetArrayValues<int>(val->GetArray()->GetValue()));
  val = schema.GetProp("param18")->GetDefaultValue();
  EXPECT_TRUE(val->GetArray()->GetValue().empty());
}

TEST(CommandSchema, ObjectSchema_FromJson_Shorthand_TypeDeduction_Array) {
  ObjectSchema schema;
  const char* schema_str = "{"
      "'param1' :[0,1,2,3],"
      "'param2' :[0.0,1.1,2.2],"
      "'param3' :['id1', 'id2'],"
      "'param4' :{'enum':[1,2,3]},"
      "'param5' :{'enum':[-1.1,2.2,3]},"
      "'param6' :{'enum':['id0', 'id1']},"
      "'param7' :{'type':'integer', 'enum':[1,2,3]},"
      "'param8' :{'type':'number',  'enum':[1,2,3]},"
      "'param9' :{'type':'number',  'enum':[]},"
      "'param10':{'type':'integer', 'enum':[]},"
      "'param11':[[0,1],[2,3]],"
      "'param12':[['foo','bar']],"
      "'param13':{'enum':[['id0', 'id1']]}"
     "}";
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                              nullptr));
  EXPECT_EQ("integer", schema.GetProp("param1")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param2")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param3")->GetTypeAsString());
  EXPECT_EQ("integer", schema.GetProp("param4")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param5")->GetTypeAsString());
  EXPECT_EQ("string", schema.GetProp("param6")->GetTypeAsString());
  EXPECT_EQ("integer", schema.GetProp("param7")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param8")->GetTypeAsString());
  EXPECT_EQ("number", schema.GetProp("param9")->GetTypeAsString());
  EXPECT_EQ("integer", schema.GetProp("param10")->GetTypeAsString());

  auto prop_type11 = schema.GetProp("param11");
  EXPECT_EQ("array", prop_type11->GetTypeAsString());
  EXPECT_EQ("integer",
            prop_type11->GetArray()->GetItemTypePtr()->GetTypeAsString());

  auto prop_type12 = schema.GetProp("param12");
  EXPECT_EQ("array", prop_type12->GetTypeAsString());
  EXPECT_EQ("string",
            prop_type12->GetArray()->GetItemTypePtr()->GetTypeAsString());

  auto prop_type13 = schema.GetProp("param13");
  EXPECT_EQ("array", prop_type12->GetTypeAsString());
  EXPECT_EQ("string",
            prop_type13->GetArray()->GetItemTypePtr()->GetTypeAsString());

  EXPECT_EQ((std::vector<int>{0, 1, 2, 3}),
            GetOneOfValues<int>(schema.GetProp("param1")));
  EXPECT_EQ((std::vector<double>{0.0, 1.1, 2.2}),
            GetOneOfValues<double>(schema.GetProp("param2")));
  EXPECT_EQ((std::vector<std::string>{"id1", "id2"}),
            GetOneOfValues<std::string>(schema.GetProp("param3")));

  EXPECT_EQ((std::vector<int>{1, 2, 3}),
            GetOneOfValues<int>(schema.GetProp("param4")));
  EXPECT_EQ((std::vector<double>{-1.1, 2.2, 3.0}),
            GetOneOfValues<double>(schema.GetProp("param5")));
  EXPECT_EQ((std::vector<std::string>{"id0", "id1"}),
            GetOneOfValues<std::string>(schema.GetProp("param6")));
  EXPECT_EQ((std::vector<int>{1, 2, 3}),
            GetOneOfValues<int>(schema.GetProp("param7")));
  EXPECT_EQ((std::vector<double>{1.0, 2.0, 3.0}),
            GetOneOfValues<double>(schema.GetProp("param8")));
  EXPECT_TRUE(GetOneOfValues<double>(schema.GetProp("param9")).empty());
  EXPECT_TRUE(GetOneOfValues<int>(schema.GetProp("param10")).empty());
}

TEST(CommandSchema, ObjectSchema_FromJson_Inheritance) {
  const char* base_schema_str = "{"
      "'param0' :{'minimum':1, 'maximum':5},"
      "'param1' :{'minimum':1, 'maximum':5},"
      "'param2' :{'minimum':1, 'maximum':5},"
      "'param3' :{'minimum':1, 'maximum':5},"
      "'param4' :{'minimum':1, 'maximum':5},"
      "'param5' :{'minimum':1.1, 'maximum':5.5},"
      "'param6' :{'minimum':1.1, 'maximum':5.5},"
      "'param7' :{'minimum':1.1, 'maximum':5.5},"
      "'param8' :{'minimum':1.1, 'maximum':5.5},"
      "'param9' :{'minLength':1, 'maxLength':5},"
      "'param10':{'minLength':1, 'maxLength':5},"
      "'param11':{'minLength':1, 'maxLength':5},"
      "'param12':{'minLength':1, 'maxLength':5},"
      "'param13':[1,2,3],"
      "'param14':[1,2,3],"
      "'param15':[1.1,2.2,3.3],"
      "'param16':[1.1,2.2,3.3],"
      "'param17':['id1', 'id2'],"
      "'param18':['id1', 'id2'],"
      "'param19':{'minimum':1, 'maximum':5},"
      "'param20':{'default':49},"
      "'param21':{'default':49},"
      "'param22':'integer'"
      "}";
  ObjectSchema base_schema;
  EXPECT_TRUE(base_schema.FromJson(CreateDictionaryValue(base_schema_str).get(),
                                   nullptr, nullptr));
  const char* schema_str = "{"
      "'param1' :{},"
      "'param2' :{'minimum':2},"
      "'param3' :{'maximum':9},"
      "'param4' :{'minimum':2, 'maximum':9},"
      "'param5' :{},"
      "'param6' :{'minimum':2.2},"
      "'param7' :{'maximum':9.9},"
      "'param8' :{'minimum':2.2, 'maximum':9.9},"
      "'param9' :{},"
      "'param10':{'minLength':3},"
      "'param11':{'maxLength':8},"
      "'param12':{'minLength':3, 'maxLength':8},"
      "'param13':{},"
      "'param14':[1,2,3,4],"
      "'param15':{},"
      "'param16':[1.1,2.2,3.3,4.4],"
      "'param17':{},"
      "'param18':['id1', 'id3'],"
      "'param19':{},"
      "'param20':{},"
      "'param21':{'default':8},"
      "'param22':{'default':123}"
      "}";
  ObjectSchema schema;
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(),
                              &base_schema, nullptr));
  EXPECT_EQ(nullptr, schema.GetProp("param0"));
  EXPECT_NE(nullptr, schema.GetProp("param1"));
  EXPECT_EQ("integer", schema.GetProp("param1")->GetTypeAsString());
  EXPECT_EQ(1, schema.GetProp("param1")->GetInt()->GetMinValue());
  EXPECT_EQ(5, schema.GetProp("param1")->GetInt()->GetMaxValue());
  EXPECT_EQ("integer", schema.GetProp("param2")->GetTypeAsString());
  EXPECT_EQ(2, schema.GetProp("param2")->GetInt()->GetMinValue());
  EXPECT_EQ(5, schema.GetProp("param2")->GetInt()->GetMaxValue());
  EXPECT_EQ("integer", schema.GetProp("param3")->GetTypeAsString());
  EXPECT_EQ(1, schema.GetProp("param3")->GetInt()->GetMinValue());
  EXPECT_EQ(9, schema.GetProp("param3")->GetInt()->GetMaxValue());
  EXPECT_EQ("integer", schema.GetProp("param4")->GetTypeAsString());
  EXPECT_EQ(2, schema.GetProp("param4")->GetInt()->GetMinValue());
  EXPECT_EQ(9, schema.GetProp("param4")->GetInt()->GetMaxValue());
  EXPECT_EQ("number", schema.GetProp("param5")->GetTypeAsString());
  EXPECT_EQ(1.1, schema.GetProp("param5")->GetDouble()->GetMinValue());
  EXPECT_EQ(5.5, schema.GetProp("param5")->GetDouble()->GetMaxValue());
  EXPECT_EQ("number", schema.GetProp("param6")->GetTypeAsString());
  EXPECT_EQ(2.2, schema.GetProp("param6")->GetDouble()->GetMinValue());
  EXPECT_EQ(5.5, schema.GetProp("param6")->GetDouble()->GetMaxValue());
  EXPECT_EQ("number", schema.GetProp("param7")->GetTypeAsString());
  EXPECT_EQ(1.1, schema.GetProp("param7")->GetDouble()->GetMinValue());
  EXPECT_EQ(9.9, schema.GetProp("param7")->GetDouble()->GetMaxValue());
  EXPECT_EQ("number", schema.GetProp("param8")->GetTypeAsString());
  EXPECT_EQ(2.2, schema.GetProp("param8")->GetDouble()->GetMinValue());
  EXPECT_EQ(9.9, schema.GetProp("param8")->GetDouble()->GetMaxValue());
  EXPECT_EQ("string", schema.GetProp("param9")->GetTypeAsString());
  EXPECT_EQ(1, schema.GetProp("param9")->GetString()->GetMinLength());
  EXPECT_EQ(5, schema.GetProp("param9")->GetString()->GetMaxLength());
  EXPECT_EQ("string", schema.GetProp("param10")->GetTypeAsString());
  EXPECT_EQ(3, schema.GetProp("param10")->GetString()->GetMinLength());
  EXPECT_EQ(5, schema.GetProp("param10")->GetString()->GetMaxLength());
  EXPECT_EQ("string", schema.GetProp("param11")->GetTypeAsString());
  EXPECT_EQ(1, schema.GetProp("param11")->GetString()->GetMinLength());
  EXPECT_EQ(8, schema.GetProp("param11")->GetString()->GetMaxLength());
  EXPECT_EQ("string", schema.GetProp("param12")->GetTypeAsString());
  EXPECT_EQ(3, schema.GetProp("param12")->GetString()->GetMinLength());
  EXPECT_EQ(8, schema.GetProp("param12")->GetString()->GetMaxLength());
  EXPECT_EQ("integer", schema.GetProp("param13")->GetTypeAsString());
  EXPECT_EQ((std::vector<int>{1, 2, 3}),
            GetOneOfValues<int>(schema.GetProp("param13")));
  EXPECT_EQ("integer", schema.GetProp("param14")->GetTypeAsString());
  EXPECT_EQ((std::vector<int>{1, 2, 3, 4}),
            GetOneOfValues<int>(schema.GetProp("param14")));
  EXPECT_EQ("number", schema.GetProp("param15")->GetTypeAsString());
  EXPECT_EQ((std::vector<double>{1.1, 2.2, 3.3}),
            GetOneOfValues<double>(schema.GetProp("param15")));
  EXPECT_EQ("number", schema.GetProp("param16")->GetTypeAsString());
  EXPECT_EQ((std::vector<double>{1.1, 2.2, 3.3, 4.4}),
            GetOneOfValues<double>(schema.GetProp("param16")));
  EXPECT_EQ("string", schema.GetProp("param17")->GetTypeAsString());
  EXPECT_EQ((std::vector<std::string>{"id1", "id2"}),
            GetOneOfValues<std::string>(schema.GetProp("param17")));
  EXPECT_EQ("string", schema.GetProp("param18")->GetTypeAsString());
  EXPECT_EQ((std::vector<std::string>{"id1", "id3"}),
            GetOneOfValues<std::string>(schema.GetProp("param18")));
  EXPECT_EQ("integer", schema.GetProp("param19")->GetTypeAsString());
  EXPECT_EQ(1, schema.GetProp("param19")->GetInt()->GetMinValue());
  EXPECT_EQ(5, schema.GetProp("param19")->GetInt()->GetMaxValue());
  EXPECT_EQ(49,
            schema.GetProp("param20")->GetDefaultValue()->GetInt()->GetValue());
  EXPECT_EQ(8,
            schema.GetProp("param21")->GetDefaultValue()->GetInt()->GetValue());
  EXPECT_EQ(123,
            schema.GetProp("param22")->GetDefaultValue()->GetInt()->GetValue());
}

TEST(CommandSchema, ObjectSchema_UseDefaults) {
  ObjectPropType prop;
  const char* schema_str = "{'properties':{"
      "'param1':{'default':true},"
      "'param2':{'default':2},"
      "'param3':{'default':3.3},"
      "'param4':{'default':'four'},"
      "'param5':{'default':{'x':5,'y':6},"
                "'properties':{'x':'integer','y':'integer'}},"
      "'param6':{'default':[1,2,3]}"
      "}}";
  ASSERT_TRUE(prop.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                            nullptr));

  // Omit all.
  auto value = prop.CreateValue();
  ASSERT_TRUE(value->FromJson(CreateDictionaryValue("{}").get(), nullptr));
  native_types::Object obj = value->GetObject()->GetValue();
  EXPECT_TRUE(obj["param1"]->GetBoolean()->GetValue());
  EXPECT_EQ(2, obj["param2"]->GetInt()->GetValue());
  EXPECT_DOUBLE_EQ(3.3, obj["param3"]->GetDouble()->GetValue());
  EXPECT_EQ("four", obj["param4"]->GetString()->GetValue());
  native_types::Object param5 = obj["param5"]->GetObject()->GetValue();
  EXPECT_EQ(5, param5["x"]->GetInt()->GetValue());
  EXPECT_EQ(6, param5["y"]->GetInt()->GetValue());
  native_types::Array param6 = obj["param6"]->GetArray()->GetValue();
  EXPECT_EQ((std::vector<int>{1, 2, 3}), GetArrayValues<int>(param6));

  // Specify some.
  value = prop.CreateValue();
  const char* val_json = "{"
      "'param1':false,"
      "'param3':33.3,"
      "'param5':{'x':-5,'y':-6}"
      "}";
  ASSERT_TRUE(value->FromJson(CreateDictionaryValue(val_json).get(), nullptr));
  obj = value->GetObject()->GetValue();
  EXPECT_FALSE(obj["param1"]->GetBoolean()->GetValue());
  EXPECT_EQ(2, obj["param2"]->GetInt()->GetValue());
  EXPECT_DOUBLE_EQ(33.3, obj["param3"]->GetDouble()->GetValue());
  EXPECT_EQ("four", obj["param4"]->GetString()->GetValue());
  param5 = obj["param5"]->GetObject()->GetValue();
  EXPECT_EQ(-5, param5["x"]->GetInt()->GetValue());
  EXPECT_EQ(-6, param5["y"]->GetInt()->GetValue());
  param6 = obj["param6"]->GetArray()->GetValue();
  EXPECT_EQ((std::vector<int>{1, 2, 3}), GetArrayValues<int>(param6));

  // Specify all.
  value = prop.CreateValue();
  val_json = "{"
      "'param1':false,"
      "'param2':22,"
      "'param3':333.3,"
      "'param4':'FOUR',"
      "'param5':{'x':-55,'y':66},"
      "'param6':[-1, 0]"
      "}";
  ASSERT_TRUE(value->FromJson(CreateDictionaryValue(val_json).get(), nullptr));
  obj = value->GetObject()->GetValue();
  EXPECT_FALSE(obj["param1"]->GetBoolean()->GetValue());
  EXPECT_EQ(22, obj["param2"]->GetInt()->GetValue());
  EXPECT_DOUBLE_EQ(333.3, obj["param3"]->GetDouble()->GetValue());
  EXPECT_EQ("FOUR", obj["param4"]->GetString()->GetValue());
  param5 = obj["param5"]->GetObject()->GetValue();
  EXPECT_EQ(-55, param5["x"]->GetInt()->GetValue());
  EXPECT_EQ(66, param5["y"]->GetInt()->GetValue());
  param6 = obj["param6"]->GetArray()->GetValue();
  EXPECT_EQ((std::vector<int>{-1, 0}), GetArrayValues<int>(param6));
}

TEST(CommandSchema, ObjectSchema_FromJson_BaseSchema_Failures) {
  ObjectSchema schema;
  chromeos::ErrorPtr error;
  const char* schema_str = "{"
      "'param1':{}"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("no_type_info", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'type':'foo'}"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("unknown_type", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':[]"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("no_type_info", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'minimum':'foo'}"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("type_mismatch", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':[1,2.2]"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("type_mismatch", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'minimum':1, 'enum':[1,2,3]}"  // can't have min/max & enum.
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("unexpected_parameter", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'maximum':1, 'blah':2}"  // 'blah' is unexpected.
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("unexpected_parameter", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'enum':[1,2,3],'default':5}"  // 'default' must be 1, 2, or 3.
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("out_of_range", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':[[1,2.3]]"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("type_mismatch", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':[[1,2],[3,4],['blah']]"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("type_mismatch", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'default':[]}"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("no_type_info", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':[[[1]],[[2]]]"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("no_type_info", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'enum':[[['foo']]]}"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("no_type_info", error->GetFirstError()->GetCode());
  error.reset();

  schema_str = "{"
      "'param1':{'default':[[1],[2]]}"
      "}";
  EXPECT_FALSE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                               &error));
  EXPECT_EQ("no_type_info", error->GetFirstError()->GetCode());
  error.reset();
}

}  // namespace buffet

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/values.h>
#include <gtest/gtest.h>

#include "buffet/commands/object_schema.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/schema_constants.h"
#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateValue;
using buffet::unittests::CreateDictionaryValue;
using buffet::unittests::ValueToString;

TEST(CommandSchema, IntPropType_Empty) {
  buffet::IntPropType prop;
  EXPECT_TRUE(prop.GetConstraints().empty());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
}

TEST(CommandSchema, IntPropType_Types) {
  buffet::IntPropType prop;
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(&prop, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
}

TEST(CommandSchema, IntPropType_ToJson) {
  buffet::IntPropType prop;
  EXPECT_EQ("'integer'", ValueToString(prop.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'type':'integer'}",
            ValueToString(prop.ToJson(true, nullptr).get()));
  buffet::IntPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_EQ("{}", ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'minimum':3}").get(),
                  &prop, nullptr);
  EXPECT_EQ("{'minimum':3}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'maximum':-7}").get(),
                  &prop, nullptr);
  EXPECT_EQ("{'maximum':-7}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'minimum':0,'maximum':5}").get(),
                  &prop, nullptr);
  EXPECT_EQ("{'maximum':5,'minimum':0}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'enum':[1,2,3]}").get(), &prop,
                  nullptr);
  EXPECT_EQ("[1,2,3]",
            ValueToString(param2.ToJson(false, nullptr).get()));
}

TEST(CommandSchema, IntPropType_FromJson) {
  buffet::IntPropType prop;
  prop.AddMinMaxConstraint(2, 8);
  buffet::IntPropType param2;
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
  param2.FromJson(CreateDictionaryValue("{'minimum':0,'maximum':6}").get(),
                  &prop, nullptr);
  EXPECT_TRUE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(0, param2.GetMinValue());
  EXPECT_EQ(6, param2.GetMaxValue());
}

TEST(CommandSchema, IntPropType_Validate) {
  buffet::IntPropType prop;
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
  buffet::IntPropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(2, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(2, val->GetValueAsAny().Get<int>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, BoolPropType_Empty) {
  buffet::BooleanPropType prop;
  EXPECT_TRUE(prop.GetConstraints().empty());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
}

TEST(CommandSchema, BoolPropType_Types) {
  buffet::BooleanPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(&prop, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
}

TEST(CommandSchema, BoolPropType_ToJson) {
  buffet::BooleanPropType prop;
  EXPECT_EQ("'boolean'", ValueToString(prop.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'type':'boolean'}",
            ValueToString(prop.ToJson(true, nullptr).get()));
  buffet::BooleanPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_EQ("{}", ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'enum':[true,false]}").get(), &prop,
                  nullptr);
  EXPECT_EQ("[true,false]", ValueToString(param2.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'enum':[true,false],'type':'boolean'}",
            ValueToString(param2.ToJson(true, nullptr).get()));
}

TEST(CommandSchema, BoolPropType_FromJson) {
  buffet::BooleanPropType prop;
  prop.FromJson(CreateDictionaryValue("{'enum':[true]}").get(), &prop,
                nullptr);
  buffet::BooleanPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_FALSE(param2.HasOverriddenAttributes());
  EXPECT_TRUE(param2.IsBasedOnSchema());
  EXPECT_EQ(std::vector<bool>{true}, prop.GetOneOfValues());
}

TEST(CommandSchema, BoolPropType_Validate) {
  buffet::BooleanPropType prop;
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
  buffet::BooleanPropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(true, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_TRUE(val->GetValueAsAny().Get<bool>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, DoublePropType_Empty) {
  buffet::DoublePropType prop;
  EXPECT_DOUBLE_EQ(std::numeric_limits<double>::lowest(), prop.GetMinValue());
  EXPECT_DOUBLE_EQ((std::numeric_limits<double>::max)(), prop.GetMaxValue());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
}

TEST(CommandSchema, DoublePropType_Types) {
  buffet::DoublePropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(&prop, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
}

TEST(CommandSchema, DoublePropType_ToJson) {
  buffet::DoublePropType prop;
  EXPECT_EQ("'number'", ValueToString(prop.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'type':'number'}",
            ValueToString(prop.ToJson(true, nullptr).get()));
  buffet::DoublePropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_EQ("{}", ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'minimum':3}").get(), &prop,
                  nullptr);
  EXPECT_EQ("{'minimum':3.0}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'maximum':-7}").get(), &prop,
                  nullptr);
  EXPECT_EQ("{'maximum':-7.0}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'minimum':0,'maximum':5}").get(),
                  &prop, nullptr);
  EXPECT_EQ("{'maximum':5.0,'minimum':0.0}",
            ValueToString(param2.ToJson(false, nullptr).get()));
}

TEST(CommandSchema, DoublePropType_FromJson) {
  buffet::DoublePropType prop;
  prop.AddMinMaxConstraint(2.5, 8.7);
  buffet::DoublePropType param2;
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
}

TEST(CommandSchema, DoublePropType_Validate) {
  buffet::DoublePropType prop;
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
  buffet::DoublePropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(2.0, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_DOUBLE_EQ(2.0, val->GetValueAsAny().Get<double>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, StringPropType_Empty) {
  buffet::StringPropType prop;
  EXPECT_EQ(0, prop.GetMinLength());
  EXPECT_EQ((std::numeric_limits<int>::max)(), prop.GetMaxLength());
  EXPECT_FALSE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
}

TEST(CommandSchema, StringPropType_Types) {
  buffet::StringPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(&prop, prop.GetString());
  EXPECT_EQ(nullptr, prop.GetObject());
}

TEST(CommandSchema, StringPropType_ToJson) {
  buffet::StringPropType prop;
  EXPECT_EQ("'string'", ValueToString(prop.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'type':'string'}",
            ValueToString(prop.ToJson(true, nullptr).get()));
  buffet::StringPropType param2;
  param2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_EQ("{}", ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'minLength':3}").get(), &prop,
                  nullptr);
  EXPECT_EQ("{'minLength':3}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'maxLength':7}").get(), &prop,
                  nullptr);
  EXPECT_EQ("{'maxLength':7}",
            ValueToString(param2.ToJson(false, nullptr).get()));
  param2.FromJson(CreateDictionaryValue("{'minLength':0,'maxLength':5}").get(),
                  &prop, nullptr);
  EXPECT_EQ("{'maxLength':5,'minLength':0}",
            ValueToString(param2.ToJson(false, nullptr).get()));
}

TEST(CommandSchema, StringPropType_FromJson) {
  buffet::StringPropType prop;
  prop.AddLengthConstraint(2, 8);
  buffet::StringPropType param2;
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
}

TEST(CommandSchema, StringPropType_Validate) {
  buffet::StringPropType prop;
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
  buffet::StringPropType prop;
  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(std::string{"blah"}, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ("blah", val->GetValueAsAny().Get<std::string>());

  val = prop.CreateValue(4, &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, ObjectPropType_Empty) {
  buffet::ObjectPropType prop;
  EXPECT_TRUE(prop.HasOverriddenAttributes());
  EXPECT_FALSE(prop.IsBasedOnSchema());
}

TEST(CommandSchema, ObjectPropType_Types) {
  buffet::ObjectPropType prop;
  EXPECT_EQ(nullptr, prop.GetInt());
  EXPECT_EQ(nullptr, prop.GetBoolean());
  EXPECT_EQ(nullptr, prop.GetDouble());
  EXPECT_EQ(nullptr, prop.GetString());
  EXPECT_EQ(&prop, prop.GetObject());
}

TEST(CommandSchema, ObjectPropType_ToJson) {
  buffet::ObjectPropType prop;
  EXPECT_EQ("{'properties':{}}",
            ValueToString(prop.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'properties':{},'type':'object'}",
            ValueToString(prop.ToJson(true, nullptr).get()));
  EXPECT_FALSE(prop.IsBasedOnSchema());
  buffet::ObjectPropType prop2;
  prop2.FromJson(CreateDictionaryValue("{}").get(), &prop, nullptr);
  EXPECT_EQ("{}", ValueToString(prop2.ToJson(false, nullptr).get()));
  EXPECT_TRUE(prop2.IsBasedOnSchema());

  auto schema = std::make_shared<buffet::ObjectSchema>();
  schema->AddProp("expires", std::make_shared<buffet::IntPropType>());
  auto pw = std::make_shared<buffet::StringPropType>();
  pw->AddLengthConstraint(6, 100);
  schema->AddProp("password", pw);
  prop2.SetObjectSchema(schema);
  EXPECT_EQ("{'properties':{'expires':'integer',"
            "'password':{'maxLength':100,'minLength':6}}}",
            ValueToString(prop2.ToJson(false, nullptr).get()));
  EXPECT_EQ("{'properties':{'expires':{'type':'integer'},"
            "'password':{'maxLength':100,'minLength':6,'type':'string'}},"
            "'type':'object'}",
            ValueToString(prop2.ToJson(true, nullptr).get()));
}

TEST(CommandSchema, ObjectPropType_FromJson) {
  buffet::ObjectPropType base_prop;
  EXPECT_TRUE(base_prop.FromJson(CreateDictionaryValue(
      "{'properties':{'name':'string','age':'integer'}}").get(), nullptr,
      nullptr));
  auto schema = base_prop.GetObjectSchemaPtr();
  const buffet::PropType* prop = schema->GetProp("name");
  EXPECT_EQ(buffet::ValueType::String, prop->GetType());
  prop = schema->GetProp("age");
  EXPECT_EQ(buffet::ValueType::Int, prop->GetType());
}

TEST(CommandSchema, ObjectPropType_Validate) {
  buffet::ObjectPropType prop;
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
  buffet::ObjectPropType prop;
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
  buffet::ObjectPropType prop;
  buffet::IntPropType int_type;
  ASSERT_TRUE(prop.FromJson(CreateDictionaryValue(
      "{'properties':{'width':'integer','height':'integer'},"
      "'enum':[{'width':10,'height':20},{'width':100,'height':200}]}").get(),
      nullptr, nullptr));
  buffet::native_types::Object obj{
    {"width", int_type.CreateValue(10, nullptr)},
    {"height", int_type.CreateValue(20, nullptr)},
  };

  chromeos::ErrorPtr error;
  auto val = prop.CreateValue(obj, &error);
  ASSERT_NE(nullptr, val.get());
  EXPECT_EQ(nullptr, error.get());
  EXPECT_EQ(obj, val->GetValueAsAny().Get<buffet::native_types::Object>());

  val = prop.CreateValue("blah", &error);
  EXPECT_EQ(nullptr, val.get());
  ASSERT_NE(nullptr, error.get());
  EXPECT_EQ(buffet::errors::commands::kTypeMismatch, error->GetCode());
}

///////////////////////////////////////////////////////////////////////////////

TEST(CommandSchema, ObjectSchema_FromJson_Shorthand_TypeName) {
  buffet::ObjectSchema schema;
  const char* schema_str = "{"
  "'param1':'integer',"
  "'param2':'number',"
  "'param3':'string'"
  "}";
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                              nullptr));
  EXPECT_EQ(buffet::ValueType::Int, schema.GetProp("param1")->GetType());
  EXPECT_EQ(buffet::ValueType::Double, schema.GetProp("param2")->GetType());
  EXPECT_EQ(buffet::ValueType::String, schema.GetProp("param3")->GetType());
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
  buffet::ObjectSchema schema;
  const char* schema_str = "{"
  "'param1':{'type':'integer'},"
  "'param2':{'type':'number'},"
  "'param3':{'type':'string'}"
  "}";
  EXPECT_TRUE(schema.FromJson(CreateDictionaryValue(schema_str).get(), nullptr,
                              nullptr));
  EXPECT_EQ(buffet::ValueType::Int, schema.GetProp("param1")->GetType());
  EXPECT_EQ(buffet::ValueType::Double, schema.GetProp("param2")->GetType());
  EXPECT_EQ(buffet::ValueType::String, schema.GetProp("param3")->GetType());
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

TEST(CommandSchema, ObjectSchema_FromJson_Shorthand_TypeDeduction_Scalar) {
  buffet::ObjectSchema schema;
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
  "'param11':{'maxLength':8, 'minLength':3}"
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
}

TEST(CommandSchema, ObjectSchema_FromJson_Shorthand_TypeDeduction_Array) {
  buffet::ObjectSchema schema;
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
  "'param10':{'type':'integer', 'enum':[]}"
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

  EXPECT_EQ(4, schema.GetProp("param1")->GetInt()->GetOneOfValues().size());
  EXPECT_EQ(3, schema.GetProp("param2")->GetDouble()->GetOneOfValues().size());
  EXPECT_EQ(2, schema.GetProp("param3")->GetString()->GetOneOfValues().size());
  EXPECT_EQ(3, schema.GetProp("param4")->GetInt()->GetOneOfValues().size());
  EXPECT_EQ(3, schema.GetProp("param5")->GetDouble()->GetOneOfValues().size());
  EXPECT_EQ(2, schema.GetProp("param6")->GetString()->GetOneOfValues().size());
  EXPECT_EQ(3, schema.GetProp("param7")->GetInt()->GetOneOfValues().size());
  EXPECT_EQ(3, schema.GetProp("param8")->GetDouble()->GetOneOfValues().size());
  EXPECT_EQ(0, schema.GetProp("param9")->GetDouble()->GetOneOfValues().size());
  EXPECT_EQ(0, schema.GetProp("param10")->GetInt()->GetOneOfValues().size());

  EXPECT_EQ(std::vector<int>({0, 1, 2, 3}),
            schema.GetProp("param1")->GetInt()->GetOneOfValues());
  EXPECT_EQ(std::vector<double>({0.0, 1.1, 2.2}),
            schema.GetProp("param2")->GetDouble()->GetOneOfValues());
  EXPECT_EQ(std::vector<std::string>({"id1", "id2"}),
            schema.GetProp("param3")->GetString()->GetOneOfValues());

  EXPECT_EQ(std::vector<int>({1, 2, 3}),
            schema.GetProp("param4")->GetInt()->GetOneOfValues());
  EXPECT_EQ(std::vector<double>({-1.1, 2.2, 3.0}),
            schema.GetProp("param5")->GetDouble()->GetOneOfValues());
  EXPECT_EQ(std::vector<std::string>({"id0", "id1"}),
            schema.GetProp("param6")->GetString()->GetOneOfValues());
  EXPECT_EQ(std::vector<int>({1, 2, 3}),
            schema.GetProp("param7")->GetInt()->GetOneOfValues());
  EXPECT_EQ(std::vector<double>({1.0, 2.0, 3.0}),
            schema.GetProp("param8")->GetDouble()->GetOneOfValues());
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
  "'param19':{'minimum':1, 'maximum':5}"
  "}";
  buffet::ObjectSchema base_schema;
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
  "'param19':{}"
  "}";
  buffet::ObjectSchema schema;
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
  EXPECT_EQ(std::vector<int>({1, 2, 3}),
            schema.GetProp("param13")->GetInt()->GetOneOfValues());
  EXPECT_EQ("integer", schema.GetProp("param14")->GetTypeAsString());
  EXPECT_EQ(std::vector<int>({1, 2, 3, 4}),
            schema.GetProp("param14")->GetInt()->GetOneOfValues());
  EXPECT_EQ("number", schema.GetProp("param15")->GetTypeAsString());
  EXPECT_EQ(std::vector<double>({1.1, 2.2, 3.3}),
            schema.GetProp("param15")->GetDouble()->GetOneOfValues());
  EXPECT_EQ("number", schema.GetProp("param16")->GetTypeAsString());
  EXPECT_EQ(std::vector<double>({1.1, 2.2, 3.3, 4.4}),
            schema.GetProp("param16")->GetDouble()->GetOneOfValues());
  EXPECT_EQ("string", schema.GetProp("param17")->GetTypeAsString());
  EXPECT_EQ(std::vector<std::string>({"id1", "id2"}),
            schema.GetProp("param17")->GetString()->GetOneOfValues());
  EXPECT_EQ("string", schema.GetProp("param18")->GetTypeAsString());
  EXPECT_EQ(std::vector<std::string>({"id1", "id3"}),
            schema.GetProp("param18")->GetString()->GetOneOfValues());
  EXPECT_EQ("integer", schema.GetProp("param19")->GetTypeAsString());
  EXPECT_EQ(1, schema.GetProp("param19")->GetInt()->GetMinValue());
  EXPECT_EQ(5, schema.GetProp("param19")->GetInt()->GetMaxValue());
}

TEST(CommandSchema, ObjectSchema_FromJson_BaseSchema_Failures) {
  buffet::ObjectSchema schema;
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
}


// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/json/json_reader.h>
#include <base/values.h>
#include <brillo/map_utils.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "runtime_probe/probe_result_checker.h"

namespace runtime_probe {

typedef FieldConverter::ReturnCode ReturnCode;

TEST(StringFieldConverterTest, TestIntToString) {
  base::DictionaryValue dict_value;

  dict_value.SetInteger("key", 123);

  auto converter = StringFieldConverter::Build("");

  ASSERT_EQ(converter->Convert("key", &dict_value), ReturnCode::OK)
      << "failed to convert 123 to string";

  std::string string_value;
  ASSERT_TRUE(dict_value.GetString("key", &string_value));
  ASSERT_EQ(string_value, "123");
}

TEST(StringFieldConverterTest, TestInvalidRegexPattern) {
  auto invalid = StringFieldConverter::Build("!re hello[");
  ASSERT_EQ(invalid, nullptr);

  auto valid = StringFieldConverter::Build("!eq hello[");
  ASSERT_TRUE(valid);
}

TEST(IntegerFieldConverterTest, TestStringToInt) {
  for (const auto s : {"123", "  123", "123  ", "  123  "}) {
    base::DictionaryValue dict_value;

    dict_value.SetString("key", s);

    auto converter = IntegerFieldConverter::Build("");

    ASSERT_EQ(converter->Convert("key", &dict_value), ReturnCode::OK)
        << "failed to convert string: " << s;

    int int_value;
    ASSERT_TRUE(dict_value.GetInteger("key", &int_value));
    ASSERT_EQ(int_value, 123) << s << " is not converted to 123";
  }
}

TEST(HexFieldConverterTest, TestStringToInt) {
  for (const auto s : {"7b", "0x7b", "  0x7b", "  0x7b  ", "0x7b  "}) {
    base::DictionaryValue dict_value;

    dict_value.SetString("key", s);

    auto converter = HexFieldConverter::Build("");

    ASSERT_EQ(converter->Convert("key", &dict_value), ReturnCode::OK)
        << "failed to convert string: " << s;

    int int_value;
    ASSERT_TRUE(dict_value.GetInteger("key", &int_value));
    ASSERT_EQ(int_value, 123) << s << " is not converted to 123";
  }
}

TEST(IntegerFieldConverterTest, TestDoubleToInt) {
  double v = 123.5;
  base::DictionaryValue dict_value;

  dict_value.SetDouble("key", v);

  auto converter = IntegerFieldConverter::Build("");

  ASSERT_EQ(converter->Convert("key", &dict_value), ReturnCode::OK)
      << "failed to convert double";

  int int_value;
  ASSERT_TRUE(dict_value.GetInteger("key", &int_value));
  ASSERT_EQ(int_value, 123) << v << " is not converted to 123";
}

TEST(DoubleFieldConverterTest, TestStringToDouble) {
  for (const auto s : {"123.5", "  123.5", "123.5  ", "  123.5  "}) {
    base::DictionaryValue dict_value;

    dict_value.SetString("key", s);

    auto converter = DoubleFieldConverter::Build("");

    ASSERT_EQ(converter->Convert("key", &dict_value), ReturnCode::OK)
        << "failed to convert string: " << s;

    double double_value;
    ASSERT_TRUE(dict_value.GetDouble("key", &double_value));
    ASSERT_EQ(double_value, 123.5) << s << " is not converted to 123.5";
  }
}

TEST(DoubleFieldConverterTest, TestInvalidStringToDouble) {
  for (const auto s : {"this is not double", "", "   "}) {
    base::DictionaryValue dict_value;

    dict_value.SetString("key", s);

    auto converter = DoubleFieldConverter::Build("");

    ASSERT_EQ(converter->Convert("key", &dict_value),
              ReturnCode::INCOMPATIBLE_VALUE)
        << "Converting " << s << " to double should fail";
  }
}

TEST(ProbeResultCheckerTest, TestFromDictionaryValue) {
  const auto json_string = R"({
    "string_field": [true, "str"],
    "string_field_exact_match": [true, "str", "!eq xx[yy"],
    "string_field_with_validate_rule": [true, "str", "!re hello_.*"],
    "int_field": [true, "int"],
    "double_field": [true, "double"],
    "hex_field": [false, "hex"]
  })";
  auto dict_value =
      base::DictionaryValue::From(base::JSONReader::Read(json_string));
  ASSERT_TRUE(dict_value.get());

  auto expect_fields = ProbeResultChecker::FromDictionaryValue(*dict_value);
  ASSERT_TRUE(expect_fields.get());

  const auto& required = expect_fields->required_fields_;
  ASSERT_THAT(brillo::GetMapKeys(required),
              ::testing::UnorderedElementsAre("string_field",
                                              "string_field_exact_match",
                                              "string_field_with_validate_rule",
                                              "int_field", "double_field"));
  ASSERT_TRUE(
      dynamic_cast<StringFieldConverter*>(required.at("string_field").get()));
  ASSERT_TRUE(dynamic_cast<StringFieldConverter*>(
      required.at("string_field_exact_match").get()));
  ASSERT_TRUE(dynamic_cast<StringFieldConverter*>(
      required.at("string_field_exact_match").get()));
  ASSERT_TRUE(dynamic_cast<StringFieldConverter*>(
      required.at("string_field_with_validate_rule").get()));
  ASSERT_TRUE(
      dynamic_cast<IntegerFieldConverter*>(required.at("int_field").get()));
  ASSERT_TRUE(
      dynamic_cast<DoubleFieldConverter*>(required.at("double_field").get()));

  const auto& optional = expect_fields->optional_fields_;
  ASSERT_THAT(brillo::GetMapKeys(optional),
              ::testing::UnorderedElementsAre("hex_field"));
  ASSERT_TRUE(dynamic_cast<HexFieldConverter*>(optional.at("hex_field").get()));
}

TEST(ProbeResultCheckerTest, TestApplySuccess) {
  const auto expect = R"({
    "str": [true, "str"],
    "int": [true, "int"],
    "hex": [true, "hex"],
    "double": [true, "double"]
  })";

  const auto probe_result_string = R"({
    "str": "string result",
    "int": "1024",
    "hex": "0x7b",
    "double": "1e2"
  })";

  auto probe_result =
      base::DictionaryValue::From(base::JSONReader::Read(probe_result_string));
  auto checker = ProbeResultChecker::FromDictionaryValue(
      *base::DictionaryValue::From(base::JSONReader::Read(expect)));

  ASSERT_TRUE(checker->Apply(probe_result.get()));

  std::string str_value;
  ASSERT_TRUE(probe_result->GetString("str", &str_value));
  ASSERT_EQ(str_value, "string result");

  int int_value;
  ASSERT_TRUE(probe_result->GetInteger("int", &int_value));
  ASSERT_EQ(int_value, 1024);

  int hex_value;
  ASSERT_TRUE(probe_result->GetInteger("hex", &hex_value));
  ASSERT_EQ(hex_value, 123);

  double double_value;
  ASSERT_TRUE(probe_result->GetDouble("double", &double_value));
  ASSERT_EQ(double_value, 100);
}

}  // namespace runtime_probe

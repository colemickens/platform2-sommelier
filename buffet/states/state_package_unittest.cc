// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/states/state_package.h"

#include <memory>
#include <string>

#include <base/values.h>
#include <chromeos/variant_dictionary.h>
#include <gtest/gtest.h>

#include "buffet/commands/schema_constants.h"
#include "buffet/commands/unittest_utils.h"
#include "buffet/states/error_codes.h"

using buffet::unittests::CreateDictionaryValue;
using buffet::unittests::ValueToString;

namespace buffet {

class StatePackageTestHelper {
 public:
  // Returns the state property definitions (types/constraints/etc).
  static const ObjectSchema& GetTypes(const StatePackage& package) {
    return package.types_;
  }
  // Returns the all state property values in this package.
  static const native_types::Object& GetValues(const StatePackage& package) {
    return package.values_;
  }
};

namespace {
std::unique_ptr<base::DictionaryValue> GetTestSchema() {
  return CreateDictionaryValue(R"({
    'light': 'boolean',
    'color': 'string',
    'direction':{'properties':{'azimuth':'number','altitude':{'maximum':90.0}}},
    'iso': [50, 100, 200, 400]
  })");
}

std::unique_ptr<base::DictionaryValue> GetTestValues() {
  return CreateDictionaryValue(R"({
      'light': true,
      'color': 'white',
      'direction': {'azimuth':57.2957795, 'altitude':89.9},
      'iso': 200
  })");
}

inline const ObjectSchema& GetTypes(const StatePackage& package) {
  return StatePackageTestHelper::GetTypes(package);
}
// Returns the all state property values in this package.
inline const native_types::Object& GetValues(const StatePackage& package) {
  return StatePackageTestHelper::GetValues(package);
}

}  // anonymous namespace

class StatePackageTest : public ::testing::Test {
 public:
  void SetUp() override {
    package_.reset(new StatePackage("test"));
    ASSERT_TRUE(package_->AddSchemaFromJson(GetTestSchema().get(), nullptr));
    ASSERT_TRUE(package_->AddValuesFromJson(GetTestValues().get(), nullptr));
  }
  void TearDown() override {
    package_.reset();
  }
  std::unique_ptr<StatePackage> package_;
};

TEST(StatePackage, Empty) {
  StatePackage package("test");
  EXPECT_EQ("test", package.GetName());
  EXPECT_TRUE(GetTypes(package).GetProps().empty());
  EXPECT_TRUE(GetValues(package).empty());
}

TEST(StatePackage, AddSchemaFromJson_OnEmpty) {
  StatePackage package("test");
  ASSERT_TRUE(package.AddSchemaFromJson(GetTestSchema().get(), nullptr));
  EXPECT_EQ(4, GetTypes(package).GetProps().size());
  EXPECT_EQ(4, GetValues(package).size());
  EXPECT_EQ("{'color':{'type':'string'},"
            "'direction':{'additionalProperties':false,'properties':{"
              "'altitude':{'maximum':90.0,'type':'number'},"
              "'azimuth':{'type':'number'}},"
              "'type':'object'},"
            "'iso':{'enum':[50,100,200,400],'type':'integer'},"
            "'light':{'type':'boolean'}}",
            ValueToString(GetTypes(package).ToJson(true, nullptr).get()));
  EXPECT_EQ("{'color':'','direction':{},'iso':0,'light':false}",
            ValueToString(package.GetValuesAsJson(nullptr).get()));
}

TEST(StatePackage, AddValuesFromJson_OnEmpty) {
  StatePackage package("test");
  ASSERT_TRUE(package.AddSchemaFromJson(GetTestSchema().get(), nullptr));
  ASSERT_TRUE(package.AddValuesFromJson(GetTestValues().get(), nullptr));
  EXPECT_EQ(4, GetValues(package).size());
  EXPECT_EQ("{'color':'white',"
            "'direction':{'altitude':89.9,'azimuth':57.2957795},"
            "'iso':200,"
            "'light':true}",
            ValueToString(package.GetValuesAsJson(nullptr).get()));
}

TEST_F(StatePackageTest, AddSchemaFromJson_AddMore) {
  auto dict = CreateDictionaryValue("{'brightness':['low', 'medium', 'high']}");
  ASSERT_TRUE(package_->AddSchemaFromJson(dict.get(), nullptr));
  EXPECT_EQ(5, GetTypes(*package_).GetProps().size());
  EXPECT_EQ(5, GetValues(*package_).size());
  EXPECT_EQ("{'brightness':{'enum':['low','medium','high'],'type':'string'},"
            "'color':{'type':'string'},"
            "'direction':{'additionalProperties':false,'properties':{"
              "'altitude':{'maximum':90.0,'type':'number'},"
              "'azimuth':{'type':'number'}},"
              "'type':'object'},"
            "'iso':{'enum':[50,100,200,400],'type':'integer'},"
            "'light':{'type':'boolean'}}",
            ValueToString(GetTypes(*package_).ToJson(true, nullptr).get()));
  EXPECT_EQ("{'brightness':'',"
            "'color':'white',"
            "'direction':{'altitude':89.9,'azimuth':57.2957795},"
            "'iso':200,"
            "'light':true}",
            ValueToString(package_->GetValuesAsJson(nullptr).get()));
}

TEST_F(StatePackageTest, AddValuesFromJson_AddMore) {
  auto dict = CreateDictionaryValue("{'brightness':['low', 'medium', 'high']}");
  ASSERT_TRUE(package_->AddSchemaFromJson(dict.get(), nullptr));
  dict = CreateDictionaryValue("{'brightness':'medium'}");
  ASSERT_TRUE(package_->AddValuesFromJson(dict.get(), nullptr));
  EXPECT_EQ(5, GetValues(*package_).size());
  EXPECT_EQ("{'brightness':'medium',"
            "'color':'white',"
            "'direction':{'altitude':89.9,'azimuth':57.2957795},"
            "'iso':200,"
            "'light':true}",
            ValueToString(package_->GetValuesAsJson(nullptr).get()));
}

TEST_F(StatePackageTest, AddSchemaFromJson_Error_Redefined) {
  auto dict = CreateDictionaryValue("{'color':['white', 'blue', 'red']}");
  chromeos::ErrorPtr error;
  EXPECT_FALSE(package_->AddSchemaFromJson(dict.get(), &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyRedefinition, error->GetCode());
  EXPECT_EQ("State property 'test.color' is already defined",
            error->GetMessage());
}

TEST_F(StatePackageTest, AddValuesFromJson_Error_Undefined) {
  auto dict = CreateDictionaryValue("{'brightness':'medium'}");
  chromeos::ErrorPtr error;
  EXPECT_FALSE(package_->AddValuesFromJson(dict.get(), &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
  EXPECT_EQ("State property 'test.brightness' is not defined",
            error->GetMessage());
}

TEST_F(StatePackageTest, GetPropertyValue) {
  chromeos::Any value = package_->GetPropertyValue("color", nullptr);
  EXPECT_EQ("white", value.TryGet<std::string>());

  value = package_->GetPropertyValue("light", nullptr);
  EXPECT_TRUE(value.TryGet<bool>());

  value = package_->GetPropertyValue("iso", nullptr);
  EXPECT_EQ(200, value.TryGet<int>());

  value = package_->GetPropertyValue("direction", nullptr);
  auto direction = value.TryGet<chromeos::VariantDictionary>();
  ASSERT_FALSE(direction.empty());
  EXPECT_DOUBLE_EQ(89.9, direction["altitude"].TryGet<double>());
  EXPECT_DOUBLE_EQ(57.2957795, direction["azimuth"].TryGet<double>());
}

TEST_F(StatePackageTest, GetPropertyValue_Unknown) {
  chromeos::ErrorPtr error;
  chromeos::Any value = package_->GetPropertyValue("volume", &error);
  EXPECT_TRUE(value.IsEmpty());
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
  EXPECT_EQ("State property 'test.volume' is not defined",
            error->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Simple) {
  EXPECT_TRUE(package_->SetPropertyValue("color", std::string{"blue"},
                                         nullptr));
  chromeos::Any value = package_->GetPropertyValue("color", nullptr);
  EXPECT_EQ("blue", value.TryGet<std::string>());

  EXPECT_TRUE(package_->SetPropertyValue("light", bool{false}, nullptr));
  value = package_->GetPropertyValue("light", nullptr);
  EXPECT_FALSE(value.TryGet<bool>());

  EXPECT_TRUE(package_->SetPropertyValue("iso", int{400}, nullptr));
  value = package_->GetPropertyValue("iso", nullptr);
  EXPECT_EQ(400, value.TryGet<int>());
}

TEST_F(StatePackageTest, SetPropertyValue_Object) {
  chromeos::VariantDictionary direction{
    {"altitude", double{45.0}},
    {"azimuth", double{15.0}},
  };
  EXPECT_TRUE(package_->SetPropertyValue("direction", direction, nullptr));
  EXPECT_EQ("{'color':'white',"
            "'direction':{'altitude':45.0,'azimuth':15.0},"
            "'iso':200,"
            "'light':true}",
            ValueToString(package_->GetValuesAsJson(nullptr).get()));
}

TEST_F(StatePackageTest, SetPropertyValue_Error_TypeMismatch) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(package_->SetPropertyValue("color", int{12}, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  EXPECT_EQ("Unable to convert value to type 'string'", error->GetMessage());
  error.reset();

  ASSERT_FALSE(package_->SetPropertyValue("iso", bool{false}, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kTypeMismatch, error->GetCode());
  EXPECT_EQ("Unable to convert value to type 'integer'", error->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Error_OutOfRange) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(package_->SetPropertyValue("iso", int{150}, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kOutOfRange, error->GetCode());
  EXPECT_EQ("Value 150 is invalid. Expected one of [50,100,200,400]",
            error->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Error_Object_TypeMismatch) {
  chromeos::ErrorPtr error;
  chromeos::VariantDictionary direction{
    {"altitude", double{45.0}},
    {"azimuth", int{15}},
  };
  ASSERT_FALSE(package_->SetPropertyValue("direction", direction, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kInvalidPropValue, error->GetCode());
  EXPECT_EQ("Invalid value for property 'azimuth'", error->GetMessage());
  const chromeos::Error* inner = error->GetInnerError();
  EXPECT_EQ(errors::commands::kDomain, inner->GetDomain());
  EXPECT_EQ(errors::commands::kTypeMismatch, inner->GetCode());
  EXPECT_EQ("Unable to convert value to type 'number'", inner->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Error_Object_OutOfRange) {
  chromeos::ErrorPtr error;
  chromeos::VariantDictionary direction{
    {"altitude", double{100.0}},
    {"azimuth", double{290.0}},
  };
  ASSERT_FALSE(package_->SetPropertyValue("direction", direction, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kInvalidPropValue, error->GetCode());
  EXPECT_EQ("Invalid value for property 'altitude'", error->GetMessage());
  const chromeos::Error* inner = error->GetInnerError();
  EXPECT_EQ(errors::commands::kDomain, inner->GetDomain());
  EXPECT_EQ(errors::commands::kOutOfRange, inner->GetCode());
  EXPECT_EQ("Value 100 is out of range. It must not be greater than 90",
            inner->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Error_Object_UnknownProperty) {
  chromeos::ErrorPtr error;
  chromeos::VariantDictionary direction{
    {"altitude", double{10.0}},
    {"azimuth", double{20.0}},
    {"spin", double{30.0}},
  };
  ASSERT_FALSE(package_->SetPropertyValue("direction", direction, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kUnknownProperty, error->GetCode());
  EXPECT_EQ("Unrecognized property 'spin'", error->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Error_Object_MissingProperty) {
  chromeos::ErrorPtr error;
  chromeos::VariantDictionary direction{
    {"altitude", double{10.0}},
  };
  ASSERT_FALSE(package_->SetPropertyValue("direction", direction, &error));
  EXPECT_EQ(errors::commands::kDomain, error->GetDomain());
  EXPECT_EQ(errors::commands::kPropertyMissing, error->GetCode());
  EXPECT_EQ("Required parameter missing: azimuth", error->GetMessage());
}

TEST_F(StatePackageTest, SetPropertyValue_Error_Unknown) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(package_->SetPropertyValue("volume", int{100}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
  EXPECT_EQ("State property 'test.volume' is not defined",
            error->GetMessage());
}

}  // namespace buffet

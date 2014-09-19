// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include <base/values.h>
#include <gtest/gtest.h>

#include "buffet/commands/schema_constants.h"
#include "buffet/commands/unittest_utils.h"
#include "buffet/states/error_codes.h"
#include "buffet/states/state_manager.h"

using buffet::unittests::CreateDictionaryValue;
using buffet::unittests::ValueToString;

namespace buffet {

namespace {
std::unique_ptr<base::DictionaryValue> GetTestSchema() {
  return CreateDictionaryValue(R"({
    'base': {
      'manufacturer':'string',
      'serialNumber':'string'
    },
    'terminator': {
      'target':'string'
    }
  })");
}

std::unique_ptr<base::DictionaryValue> GetTestValues() {
  return CreateDictionaryValue(R"({
    'base': {
      'manufacturer':'Skynet',
      'serialNumber':'T1000'
    }
  })");
}
}  // anonymous namespace

class StateManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    mgr_.reset(new StateManager);
    LoadStateDefinition(GetTestSchema().get(), "default", nullptr);
    ASSERT_TRUE(mgr_->LoadStateDefaults(*GetTestValues().get(), nullptr));
  }
  void TearDown() override {
    mgr_.reset();
  }

  void LoadStateDefinition(const base::DictionaryValue* json,
                           const std::string& category,
                           chromeos::ErrorPtr* error) {
    ASSERT_TRUE(mgr_->LoadStateDefinition(*json, category, error));
  }

  std::unique_ptr<StateManager> mgr_;
};

TEST(StateManager, Empty) {
  StateManager manager;
  EXPECT_TRUE(manager.GetCategories().empty());
}

TEST_F(StateManagerTest, Initialized) {
  EXPECT_EQ(std::set<std::string>{"default"}, mgr_->GetCategories());
  EXPECT_EQ("{'base':{'manufacturer':'Skynet','serialNumber':'T1000'},"
            "'terminator':{'target':''}}",
            ValueToString(mgr_->GetStateValuesAsJson(nullptr).get()));
}

TEST_F(StateManagerTest, LoadStateDefinition) {
  auto dict = CreateDictionaryValue(R"({
    'power': {
      'battery_level':'integer'
    }
  })");
  LoadStateDefinition(dict.get(), "powerd", nullptr);
  EXPECT_EQ((std::set<std::string>{"default", "powerd"}),
            mgr_->GetCategories());
  EXPECT_EQ("{'base':{'manufacturer':'Skynet','serialNumber':'T1000'},"
            "'power':{'battery_level':0},"
            "'terminator':{'target':''}}",
            ValueToString(mgr_->GetStateValuesAsJson(nullptr).get()));
}

TEST_F(StateManagerTest, SetPropertyValue) {
  ASSERT_TRUE(mgr_->SetPropertyValue("terminator.target",
                                     std::string{"John Connor"}, nullptr));
  EXPECT_EQ("{'base':{'manufacturer':'Skynet','serialNumber':'T1000'},"
            "'terminator':{'target':'John Connor'}}",
            ValueToString(mgr_->GetStateValuesAsJson(nullptr).get()));
}

TEST_F(StateManagerTest, SetPropertyValue_Error_NoName) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(mgr_->SetPropertyValue("", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNameMissing, error->GetCode());
  EXPECT_EQ("Property name is missing", error->GetMessage());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_NoPackage) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(mgr_->SetPropertyValue("target", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPackageNameMissing, error->GetCode());
  EXPECT_EQ("Package name is missing in the property name",
            error->GetMessage());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_UnknownPackage) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(mgr_->SetPropertyValue("power.level", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
  EXPECT_EQ("Unknown state property package 'power'", error->GetMessage());
}

TEST_F(StateManagerTest, SetPropertyValue_Error_UnknownProperty) {
  chromeos::ErrorPtr error;
  ASSERT_FALSE(mgr_->SetPropertyValue("base.level", int{0}, &error));
  EXPECT_EQ(errors::state::kDomain, error->GetDomain());
  EXPECT_EQ(errors::state::kPropertyNotDefined, error->GetCode());
  EXPECT_EQ("State property 'base.level' is not defined", error->GetMessage());
}

}  // namespace buffet

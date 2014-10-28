// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>  // for abs().
#include <vector>

#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "buffet/commands/schema_constants.h"
#include "buffet/commands/unittest_utils.h"
#include "buffet/states/error_codes.h"
#include "buffet/states/mock_state_change_queue_interface.h"
#include "buffet/states/state_manager.h"

using buffet::unittests::CreateDictionaryValue;
using buffet::unittests::ValueToString;
using testing::Return;
using testing::_;

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

MATCHER_P(IsStateChange, prop_set, "") {
  return arg.property_set == prop_set &&
         std::abs((arg.timestamp - base::Time::Now()).InSeconds()) < 2;
}

}  // anonymous namespace

class StateManagerTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Initial expectations.
    EXPECT_CALL(mock_state_change_queue_, IsEmpty()).Times(0);
    EXPECT_CALL(mock_state_change_queue_, NotifyPropertiesUpdated(_)).Times(0);
    EXPECT_CALL(mock_state_change_queue_, GetAndClearRecordedStateChanges())
      .Times(0);
    mgr_.reset(new StateManager(&mock_state_change_queue_));
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
  MockStateChangeQueueInterface mock_state_change_queue_;
};

TEST(StateManager, Empty) {
  MockStateChangeQueueInterface mock_state_change_queue;
  StateManager manager(&mock_state_change_queue);
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
  chromeos::VariantDictionary expected_prop_set{
    {"terminator.target", std::string{"John Connor"}},
  };
  EXPECT_CALL(mock_state_change_queue_,
              NotifyPropertiesUpdated(IsStateChange(expected_prop_set)))
      .WillOnce(Return(true));
  ASSERT_TRUE(mgr_->SetPropertyValue("terminator.target",
                                     std::string{"John Connor"}, nullptr));
  EXPECT_EQ("{'base':{'manufacturer':'Skynet','serialNumber':'T1000'},"
            "'terminator':{'target':'John Connor'}}",
            ValueToString(mgr_->GetStateValuesAsJson(nullptr).get()));
}

TEST_F(StateManagerTest, UpdateProperties) {
  chromeos::VariantDictionary prop_set{
    {"base.serialNumber", std::string{"T1000.1"}},
    {"terminator.target", std::string{"Sarah Connor"}},
  };
  EXPECT_CALL(mock_state_change_queue_,
              NotifyPropertiesUpdated(IsStateChange(prop_set)))
      .WillOnce(Return(true));
  ASSERT_TRUE(mgr_->UpdateProperties(prop_set, nullptr));
  EXPECT_EQ("{'base':{'manufacturer':'Skynet','serialNumber':'T1000.1'},"
            "'terminator':{'target':'Sarah Connor'}}",
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

TEST_F(StateManagerTest, RetrievePropertyChanges) {
  EXPECT_CALL(mock_state_change_queue_, NotifyPropertiesUpdated(_))
      .WillOnce(Return(true));
  ASSERT_TRUE(mgr_->SetPropertyValue("terminator.target",
                                     std::string{"John Connor"}, nullptr));
  std::vector<StateChange> expected_val;
  expected_val.emplace_back();
  expected_val.back().timestamp = base::Time::Now();
  expected_val.back().property_set.emplace("terminator.target",
                                           std::string{"John Connor"});
  EXPECT_CALL(mock_state_change_queue_, GetAndClearRecordedStateChanges())
      .WillOnce(Return(expected_val));
  auto changes = mgr_->GetAndClearRecordedStateChanges();
  ASSERT_EQ(1, changes.size());
  EXPECT_EQ(expected_val.back().timestamp, changes.back().timestamp);
  EXPECT_EQ(expected_val.back().property_set, changes.back().property_set);
}


}  // namespace buffet

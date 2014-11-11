// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_instance.h"

#include <gtest/gtest.h>

#include "buffet/commands/command_dictionary.h"
#include "buffet/commands/prop_types.h"
#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateDictionaryValue;
using buffet::unittests::CreateValue;

namespace {

class CommandInstanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto json = CreateDictionaryValue(R"({
      'base': {
        'reboot': {
          'parameters': {}
        }
      },
      'robot': {
        'jump': {
          'parameters': {
            'height': {
              'type': 'integer',
              'minimum': 0,
              'maximum': 100
            },
            '_jumpType': {
              'type': 'string',
              'enum': ['_withAirFlip', '_withSpin', '_withKick']
            }
          }
        },
        'speak': {
          'parameters': {
            'phrase': {
              'type': 'string',
              'enum': ['beamMeUpScotty', 'iDontDigOnSwine',
                       'iPityDaFool', 'dangerWillRobinson']
            },
            'volume': {
              'type': 'integer',
              'minimum': 0,
              'maximum': 10
            }
          }
        }
      }
    })");
    CHECK(dict_.LoadCommands(*json, "robotd", nullptr, nullptr))
        << "Failed to parse test command dictionary";
  }
  buffet::CommandDictionary dict_;
};

}  // anonymous namespace

TEST(CommandInstance, Test) {
  buffet::native_types::Object params;
  buffet::IntPropType int_prop;
  buffet::DoublePropType dbl_prop;
  params.insert(std::make_pair("freq", dbl_prop.CreateValue(800.5, nullptr)));
  params.insert(std::make_pair("volume", int_prop.CreateValue(100, nullptr)));
  buffet::CommandInstance instance("robot._beep", "robotd", params);

  EXPECT_EQ("", instance.GetID());
  EXPECT_EQ("robot._beep", instance.GetName());
  EXPECT_EQ("robotd", instance.GetCategory());
  EXPECT_EQ(params, instance.GetParameters());
  EXPECT_DOUBLE_EQ(800.5,
                   instance.FindParameter("freq")->GetDouble()->GetValue());
  EXPECT_EQ(100, instance.FindParameter("volume")->GetInt()->GetValue());
  EXPECT_EQ(nullptr, instance.FindParameter("blah").get());
}

TEST(CommandInstance, SetID) {
  buffet::CommandInstance instance("robot._beep", "robotd", {});
  instance.SetID("command_id");
  EXPECT_EQ("command_id", instance.GetID());
}

TEST_F(CommandInstanceTest, FromJson) {
  auto json = CreateDictionaryValue(R"({
    'name': 'robot.jump',
    'parameters': {
      'height': 53,
      '_jumpType': '_withKick'
    }
  })");
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, nullptr);
  EXPECT_EQ("robot.jump", instance->GetName());
  EXPECT_EQ("robotd", instance->GetCategory());
  EXPECT_EQ(53, instance->FindParameter("height")->GetInt()->GetValue());
  EXPECT_EQ("_withKick",
            instance->FindParameter("_jumpType")->GetString()->GetValue());
}

TEST_F(CommandInstanceTest, FromJson_ParamsOmitted) {
  auto json = CreateDictionaryValue("{'name': 'base.reboot'}");
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, nullptr);
  EXPECT_EQ("base.reboot", instance->GetName());
  EXPECT_EQ("robotd", instance->GetCategory());
  EXPECT_TRUE(instance->GetParameters().empty());
}

TEST_F(CommandInstanceTest, FromJson_NotObject) {
  auto json = CreateValue("'string'");
  chromeos::ErrorPtr error;
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, &error);
  EXPECT_EQ(nullptr, instance.get());
  EXPECT_EQ("json_object_expected", error->GetCode());
  EXPECT_EQ("Command instance is not a JSON object", error->GetMessage());
}

TEST_F(CommandInstanceTest, FromJson_NameMissing) {
  auto json = CreateDictionaryValue("{'param': 'value'}");
  chromeos::ErrorPtr error;
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, &error);
  EXPECT_EQ(nullptr, instance.get());
  EXPECT_EQ("parameter_missing", error->GetCode());
  EXPECT_EQ("Command name is missing", error->GetMessage());
}

TEST_F(CommandInstanceTest, FromJson_UnknownCommand) {
  auto json = CreateDictionaryValue("{'name': 'robot.scream'}");
  chromeos::ErrorPtr error;
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, &error);
  EXPECT_EQ(nullptr, instance.get());
  EXPECT_EQ("invalid_command_name", error->GetCode());
  EXPECT_EQ("Unknown command received: robot.scream", error->GetMessage());
}

TEST_F(CommandInstanceTest, FromJson_ParamsNotObject) {
  auto json = CreateDictionaryValue(R"({
    'name': 'robot.speak',
    'parameters': 'hello'
  })");
  chromeos::ErrorPtr error;
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, &error);
  EXPECT_EQ(nullptr, instance.get());
  auto inner = error->GetInnerError();
  EXPECT_EQ("json_object_expected", inner->GetCode());
  EXPECT_EQ("Property 'parameters' must be a JSON object", inner->GetMessage());
  EXPECT_EQ("command_failed", error->GetCode());
  EXPECT_EQ("Failed to validate command 'robot.speak'", error->GetMessage());
}

TEST_F(CommandInstanceTest, FromJson_ParamError) {
  auto json = CreateDictionaryValue(R"({
    'name': 'robot.speak',
    'parameters': {
      'phrase': 'iPityDaFool',
      'volume': 20
    }
  })");
  chromeos::ErrorPtr error;
  auto instance = buffet::CommandInstance::FromJson(json.get(), dict_, &error);
  EXPECT_EQ(nullptr, instance.get());
  auto first = error->GetFirstError();
  EXPECT_EQ("out_of_range", first->GetCode());
  EXPECT_EQ("Value 20 is out of range. It must not be greater than 10",
            first->GetMessage());
  auto inner = error->GetInnerError();
  EXPECT_EQ("invalid_parameter_value", inner->GetCode());
  EXPECT_EQ("Invalid value for property 'volume'",
            inner->GetMessage());
  EXPECT_EQ("command_failed", error->GetCode());
  EXPECT_EQ("Failed to validate command 'robot.speak'", error->GetMessage());
}

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/commands/command_instance.h"

#include <gtest/gtest.h>

#include "libweave/src/commands/command_dictionary.h"
#include "libweave/src/commands/prop_types.h"
#include "libweave/src/commands/schema_utils.h"
#include "libweave/src/commands/unittest_utils.h"

namespace weave {

using unittests::CreateDictionaryValue;
using unittests::CreateValue;

namespace {

class CommandInstanceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto json = CreateDictionaryValue(R"({
      'base': {
        'reboot': {
          'parameters': {},
          'results': {}
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
          },
          'results': {}
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
          },
          'results': {}
        }
      }
    })");
    CHECK(dict_.LoadCommands(*json, "robotd", nullptr, nullptr))
        << "Failed to parse test command dictionary";
  }
  CommandDictionary dict_;
};

}  // anonymous namespace

TEST_F(CommandInstanceTest, Test) {
  StringPropType str_prop;
  IntPropType int_prop;
  ValueMap params;
  params["phrase"] = str_prop.CreateValue(std::string("iPityDaFool"), nullptr);
  params["volume"] = int_prop.CreateValue(5, nullptr);
  CommandInstance instance{
      "robot.speak", "cloud", dict_.FindCommand("robot.speak"), params};

  ValueMap results;
  results["foo"] = int_prop.CreateValue(239, nullptr);
  instance.SetResults(results);

  EXPECT_EQ("", instance.GetID());
  EXPECT_EQ("robot.speak", instance.GetName());
  EXPECT_EQ("robotd", instance.GetCategory());
  EXPECT_EQ("cloud", instance.GetOrigin());
  EXPECT_EQ(params, instance.GetParameters());
  EXPECT_EQ("iPityDaFool",
            instance.FindParameter("phrase")->GetString()->GetValue());
  EXPECT_EQ(5, instance.FindParameter("volume")->GetInt()->GetValue());
  EXPECT_EQ(nullptr, instance.FindParameter("blah"));
  EXPECT_EQ(results, instance.GetResults());

  CommandInstance instance2{
      "base.reboot", "local", dict_.FindCommand("base.reboot"), {}};
  EXPECT_EQ("local", instance2.GetOrigin());
}

TEST_F(CommandInstanceTest, SetID) {
  CommandInstance instance{
      "base.reboot", "local", dict_.FindCommand("base.reboot"), {}};
  instance.SetID("command_id");
  EXPECT_EQ("command_id", instance.GetID());
}

TEST_F(CommandInstanceTest, FromJson) {
  auto json = CreateDictionaryValue(R"({
    'name': 'robot.jump',
    'id': 'abcd',
    'parameters': {
      'height': 53,
      '_jumpType': '_withKick'
    },
    'results': {}
  })");
  std::string id;
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, &id, nullptr);
  EXPECT_EQ("abcd", id);
  EXPECT_EQ("abcd", instance->GetID());
  EXPECT_EQ("robot.jump", instance->GetName());
  EXPECT_EQ("robotd", instance->GetCategory());
  EXPECT_EQ(53, instance->FindParameter("height")->GetInt()->GetValue());
  EXPECT_EQ("_withKick",
            instance->FindParameter("_jumpType")->GetString()->GetValue());
}

TEST_F(CommandInstanceTest, FromJson_ParamsOmitted) {
  auto json = CreateDictionaryValue("{'name': 'base.reboot'}");
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, nullptr);
  EXPECT_EQ("base.reboot", instance->GetName());
  EXPECT_EQ("robotd", instance->GetCategory());
  EXPECT_TRUE(instance->GetParameters().empty());
}

TEST_F(CommandInstanceTest, FromJson_NotObject) {
  auto json = CreateValue("'string'");
  chromeos::ErrorPtr error;
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, &error);
  EXPECT_EQ(nullptr, instance.get());
  EXPECT_EQ("json_object_expected", error->GetCode());
  EXPECT_EQ("Command instance is not a JSON object", error->GetMessage());
}

TEST_F(CommandInstanceTest, FromJson_NameMissing) {
  auto json = CreateDictionaryValue("{'param': 'value'}");
  chromeos::ErrorPtr error;
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, &error);
  EXPECT_EQ(nullptr, instance.get());
  EXPECT_EQ("parameter_missing", error->GetCode());
  EXPECT_EQ("Command name is missing", error->GetMessage());
}

TEST_F(CommandInstanceTest, FromJson_UnknownCommand) {
  auto json = CreateDictionaryValue("{'name': 'robot.scream'}");
  chromeos::ErrorPtr error;
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, &error);
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
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, &error);
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
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, &error);
  EXPECT_EQ(nullptr, instance.get());
  auto first = error->GetFirstError();
  EXPECT_EQ("out_of_range", first->GetCode());
  EXPECT_EQ("Value 20 is out of range. It must not be greater than 10",
            first->GetMessage());
  auto inner = error->GetInnerError();
  EXPECT_EQ("invalid_parameter_value", inner->GetCode());
  EXPECT_EQ("Invalid value for property 'volume'", inner->GetMessage());
  EXPECT_EQ("command_failed", error->GetCode());
  EXPECT_EQ("Failed to validate command 'robot.speak'", error->GetMessage());
}

TEST_F(CommandInstanceTest, ToJson) {
  auto json = CreateDictionaryValue(R"({
    'name': 'robot.jump',
    'parameters': {
      'height': 53,
      '_jumpType': '_withKick'
    },
    'results': {}
  })");
  auto instance =
      CommandInstance::FromJson(json.get(), "cloud", dict_, nullptr, nullptr);
  instance->SetProgress(
      ValueMap{{"progress", unittests::make_int_prop_value(15)}});
  instance->SetProgress(
      ValueMap{{"progress", unittests::make_int_prop_value(15)}});
  instance->SetID("testId");
  ValueMap results;
  instance->SetResults(
      ValueMap{{"testResult", unittests::make_int_prop_value(17)}});

  json->MergeDictionary(CreateDictionaryValue(R"({
    'id': 'testId',
    'progress': {'progress': 15},
    'state': 'inProgress',
    'results': {'testResult': 17}
  })").get());

  auto converted = instance->ToJson();
  EXPECT_PRED2([](const base::Value& val1, const base::Value& val2) {
    return val1.Equals(&val2);
  }, *json, *converted);
}

}  // namespace weave

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "buffet/commands/command_dictionary.h"
#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateDictionaryValue;

TEST(CommandDictionary, Empty) {
  buffet::CommandDictionary dict;
  EXPECT_TRUE(dict.IsEmpty());
  EXPECT_EQ(nullptr, dict.FindCommand("robot.jump"));
  EXPECT_TRUE(dict.GetCommandNamesByCategory("robotd").empty());
}

TEST(CommandDictionary, LoadCommands) {
  auto json = CreateDictionaryValue(R"({
    'robot': {
      'jump': {
        'parameters': {
          'height': 'integer',
          '_jumpType': ['_withAirFlip', '_withSpin', '_withKick']
        }
      }
    }
  })");
  buffet::CommandDictionary dict;
  EXPECT_TRUE(dict.LoadCommands(*json, "robotd", nullptr));
  EXPECT_EQ(1, dict.GetSize());
  EXPECT_NE(nullptr, dict.FindCommand("robot.jump"));
  json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'}
      },
      'shutdown': {
        'parameters': {}
      }
    }
  })");
  EXPECT_TRUE(dict.LoadCommands(*json, "powerd", nullptr));
  EXPECT_EQ(3, dict.GetSize());
  EXPECT_NE(nullptr, dict.FindCommand("robot.jump"));
  EXPECT_NE(nullptr, dict.FindCommand("base.reboot"));
  EXPECT_NE(nullptr, dict.FindCommand("base.shutdown"));
  EXPECT_EQ(nullptr, dict.FindCommand("foo.bar"));
  std::vector<std::string> expected_commands{"base.reboot", "base.shutdown"};
  EXPECT_EQ(expected_commands, dict.GetCommandNamesByCategory("powerd"));
}

TEST(CommandDictionary, LoadCommands_Failures) {
  buffet::CommandDictionary dict;
  buffet::ErrorPtr error;

  // Command definition missing 'parameters' property.
  auto json = CreateDictionaryValue("{'robot':{'jump':{}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", &error));
  EXPECT_EQ("parameter_missing", error->GetCode());
  EXPECT_EQ("Command definition 'robot.jump' is missing property 'parameters'",
            error->GetMessage());
  error.reset();

  // Command definition is not an object.
  json = CreateDictionaryValue("{'robot':{'jump':0}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  EXPECT_EQ("Expecting an object for command 'jump'", error->GetMessage());
  error.reset();

  // Package definition is not an object.
  json = CreateDictionaryValue("{'robot':'blah'}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", &error));
  EXPECT_EQ("type_mismatch", error->GetCode());
  EXPECT_EQ("Expecting an object for package 'robot'", error->GetMessage());
  error.reset();

  // Invalid command definition is not an object.
  json = CreateDictionaryValue("{'robot':{'jump':{'parameters':{'flip':0}}}}");
  EXPECT_FALSE(dict.LoadCommands(*json, "robotd", &error));
  EXPECT_EQ("invalid_object_schema", error->GetCode());
  EXPECT_EQ("Invalid definition for command 'robot.jump'", error->GetMessage());
  EXPECT_NE(nullptr, error->GetInnerError());  // Must have additional info.
  error.reset();

  // Redefine commands in different category.
  json = CreateDictionaryValue("{'robot':{'jump':{'parameters':{}}}}");
  dict.Clear();
  dict.LoadCommands(*json, "category1", &error);
  EXPECT_FALSE(dict.LoadCommands(*json, "category2", &error));
  EXPECT_EQ("duplicate_command_definition", error->GetCode());
  EXPECT_EQ("Definition for command 'robot.jump' overrides an earlier "
            "definition in category 'category1'", error->GetMessage());
  error.reset();
}

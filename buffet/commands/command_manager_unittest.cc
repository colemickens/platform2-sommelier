// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/commands/command_manager.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/json/json_writer.h>
#include <gtest/gtest.h>

#include "buffet/commands/unittest_utils.h"

using buffet::unittests::CreateDictionaryValue;

namespace {

const char kTestBaseCommands[] = R"({
  'base': {
    'reboot': {
      'parameters': {'delay': 'integer'},
      'results': {}
    },
    'shutdown': {
      'parameters': {},
      'results': {}
    }
  }
})";

const char kTestVendorCommands[] = R"({
  'robot': {
    '_jump': {
      'parameters': {'height': 'integer'},
      'results': {}
    },
    '_speak': {
      'parameters': {'phrase': 'string'},
      'results': {}
    }
  }
})";

const char kTestTestCommands[] = R"({
  'test': {
    '_yo': {
      'parameters': {'name': 'string'},
      'results': {}
    }
  }
})";

static void SaveJsonToFile(const base::DictionaryValue& dict,
                           const base::FilePath& file_path) {
  std::string json;
  base::JSONWriter::Write(&dict, &json);
  const int bytes_to_write = static_cast<int>(json.size());
  CHECK_EQ(bytes_to_write, WriteFile(file_path, json.data(), bytes_to_write));
}

static base::FilePath SaveJsonToTempFile(const base::DictionaryValue& dict) {
  base::FilePath temp_file;
  base::CreateTemporaryFile(&temp_file);
  SaveJsonToFile(dict, temp_file);
  return temp_file;
}

}  // namespace

TEST(CommandManager, Empty) {
  buffet::CommandManager manager;
  EXPECT_TRUE(manager.GetCommandDictionary().IsEmpty());
}

TEST(CommandManager, LoadBaseCommandsJSON) {
  buffet::CommandManager manager;
  auto json = CreateDictionaryValue(kTestBaseCommands);
  EXPECT_TRUE(manager.LoadBaseCommands(*json, nullptr));
}

TEST(CommandManager, LoadBaseCommandsFile) {
  buffet::CommandManager manager;
  auto json = CreateDictionaryValue(kTestBaseCommands);
  base::FilePath temp_file = SaveJsonToTempFile(*json);
  EXPECT_TRUE(manager.LoadBaseCommands(temp_file, nullptr));
  base::DeleteFile(temp_file, false);
}

TEST(CommandManager, LoadCommandsJSON) {
  buffet::CommandManager manager;
  auto json = CreateDictionaryValue(kTestVendorCommands);
  EXPECT_TRUE(manager.LoadCommands(*json, "category", nullptr));
}

TEST(CommandManager, LoadCommandsFile) {
  buffet::CommandManager manager;
  // Load some standard command definitions first.
  auto json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {}
      },
      'shutdown': {
        'parameters': {},
        'results': {}
      }
    }
  })");
  manager.LoadBaseCommands(*json, nullptr);
  // Load device-supported commands.
  json = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'results': {}
      }
    },
    'robot': {
      '_jump': {
        'parameters': {'height': 'integer'},
        'results': {}
      }
    }
  })");
  base::FilePath temp_file = SaveJsonToTempFile(*json);
  EXPECT_TRUE(manager.LoadCommands(temp_file, nullptr));
  base::DeleteFile(temp_file, false);
  EXPECT_EQ(2, manager.GetCommandDictionary().GetSize());
  EXPECT_NE(nullptr, manager.GetCommandDictionary().FindCommand("base.reboot"));
  EXPECT_NE(nullptr, manager.GetCommandDictionary().FindCommand("robot._jump"));
}

TEST(CommandManager, ShouldLoadStandardAndTestDefinitions) {
  buffet::CommandManager manager;
  base::ScopedTempDir temp;
  CHECK(temp.CreateUniqueTempDir());
  base::FilePath base_path{temp.path().Append("base_defs")};
  base::FilePath test_path{temp.path().Append("test_defs")};
  base::FilePath base_commands_path{base_path.Append("commands")};
  base::FilePath test_commands_path{test_path.Append("commands")};
  CHECK(CreateDirectory(base_commands_path));
  CHECK(CreateDirectory(test_commands_path));
  SaveJsonToFile(*CreateDictionaryValue(kTestBaseCommands),
                 base_path.Append("gcd.json"));
  SaveJsonToFile(*CreateDictionaryValue(kTestVendorCommands),
                 base_commands_path.Append("category.json"));
  SaveJsonToFile(*CreateDictionaryValue(kTestTestCommands),
                 test_commands_path.Append("test.json"));
  manager.Startup(base_path, test_path);
  EXPECT_EQ(3, manager.GetCommandDictionary().GetSize());
  EXPECT_NE(nullptr,
            manager.GetCommandDictionary().FindCommand("robot._jump"));
  EXPECT_NE(nullptr,
            manager.GetCommandDictionary().FindCommand("robot._speak"));
  EXPECT_NE(nullptr,
            manager.GetCommandDictionary().FindCommand("test._yo"));
}

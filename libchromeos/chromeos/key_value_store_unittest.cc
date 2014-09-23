// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/key_value_store.h>

#include <map>
#include <string>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <gtest/gtest.h>

using base::FilePath;
using base::ReadFileToString;
using std::map;
using std::string;

namespace chromeos {

class KeyValueStoreTest : public ::testing::Test {
 public:
  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_file_ = temp_dir_.path().Append("temp.conf");
  }

 protected:
  base::FilePath temp_file_;
  base::ScopedTempDir temp_dir_;
  KeyValueStore store_;  // KeyValueStore under test.
};

TEST_F(KeyValueStoreTest, CommentsAreIgnored) {
  string blob = "# comment\nA=B\n\n\n#another=comment\n\n";
  ASSERT_EQ(blob.size(), base::WriteFile(temp_file_, blob.data(), blob.size()));
  EXPECT_TRUE(store_.Load(temp_file_));

  EXPECT_TRUE(store_.Save(temp_file_));
  string read_blob;
  ASSERT_TRUE(ReadFileToString(FilePath(temp_file_), &read_blob));
  EXPECT_EQ("A=B\n", read_blob);
}

TEST_F(KeyValueStoreTest, EmptyTest) {
  ASSERT_EQ(0, base::WriteFile(temp_file_, "", 0));
  EXPECT_TRUE(store_.Load(temp_file_));

  EXPECT_TRUE(store_.Save(temp_file_));
  string read_blob;
  ASSERT_TRUE(ReadFileToString(FilePath(temp_file_), &read_blob));
  EXPECT_EQ("", read_blob);
}

TEST_F(KeyValueStoreTest, LoadAndReloadTest) {
  string blob = "A=B\nC=\n=\nFOO=BAR=BAZ\nBAR=BAX\nMISSING=NEWLINE";
  ASSERT_EQ(blob.size(), base::WriteFile(temp_file_, blob.data(), blob.size()));
  EXPECT_TRUE(store_.Load(temp_file_));

  map<string, string> expected = {
      {"A", "B"}, {"C", ""}, {"", ""}, {"FOO", "BAR=BAZ"}, {"BAR", "BAX"},
      {"MISSING", "NEWLINE"}};

  // Test expected values
  string value;
  for (const auto& it : expected) {
    EXPECT_TRUE(store_.GetString(it.first, &value));
    EXPECT_EQ(it.second, value) << "Testing key: " << it.first;
  }

  // Save, load and test again.
  EXPECT_TRUE(store_.Save(temp_file_));
  KeyValueStore new_store;
  EXPECT_TRUE(new_store.Load(temp_file_));

  for (const auto& it : expected) {
    EXPECT_TRUE(new_store.GetString(it.first, &value)) << "key: " << it.first;
    EXPECT_EQ(it.second, value) << "key: " << it.first;
  }
}

TEST_F(KeyValueStoreTest, SimpleBooleanTest) {
  bool result;
  EXPECT_FALSE(store_.GetBoolean("A", &result));

  store_.SetBoolean("A", true);
  EXPECT_TRUE(store_.GetBoolean("A", &result));
  EXPECT_TRUE(result);

  store_.SetBoolean("A", false);
  EXPECT_TRUE(store_.GetBoolean("A", &result));
  EXPECT_FALSE(result);
}

TEST_F(KeyValueStoreTest, BooleanParsingTest) {
  string blob = "TRUE=true\nfalse=false\nvar=false\nDONT_SHOUT=TRUE\n";
  ASSERT_EQ(blob.size(), base::WriteFile(temp_file_, blob.data(), blob.size()));
  EXPECT_TRUE(store_.Load(temp_file_));

  map<string, bool> expected = {
      {"TRUE", true}, {"false", false}, {"var", false}};
  bool value;
  EXPECT_FALSE(store_.GetBoolean("DONT_SHOUT", &value));
  string str_value;
  EXPECT_TRUE(store_.GetString("DONT_SHOUT", &str_value));

  // Test expected values
  for (const auto& it : expected) {
    EXPECT_TRUE(store_.GetBoolean(it.first, &value)) << "key: " << it.first;
    EXPECT_EQ(it.second, value) << "key: " << it.first;
  }
}

}  // namespace chromeos

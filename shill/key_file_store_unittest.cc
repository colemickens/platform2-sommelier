// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/key_file_store.h"

#include <base/file_util.h>
#include <base/scoped_temp_dir.h>
#include <base/stl_util.h>
#include <base/string_number_conversions.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

using file_util::FileEnumerator;
using std::set;
using std::string;
using std::vector;
using testing::Test;

namespace shill {

namespace {
const char kPlainText[] = "This is a test!";
const char kROT47Text[] = "rot47:%9:D :D 2 E6DEP";
}  // namespace {}

class KeyFileStoreTest : public Test {
 public:
  KeyFileStoreTest() : store_(&glib_) {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_file_ = temp_dir_.path().Append("test-key-file-store");
    store_.set_path(test_file_);
  }

  virtual void TearDown() {
    store_.set_path(FilePath(""));  // Don't try to save the store.
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  string ReadKeyFile();
  void WriteKeyFile(string data);
  bool OpenCheckClose(const string &group,
                      const string &key,
                      const string &expected_value);

  GLib glib_;  // Use real GLib for testing KeyFileStore.
  ScopedTempDir temp_dir_;
  FilePath test_file_;
  KeyFileStore store_;
};

string KeyFileStoreTest::ReadKeyFile() {
  string data;
  EXPECT_TRUE(file_util::ReadFileToString(store_.path(), &data));
  return data;
}

void KeyFileStoreTest::WriteKeyFile(string data) {
  EXPECT_EQ(data.size(),
            file_util::WriteFile(store_.path(), data.data(), data.size()));
}

TEST_F(KeyFileStoreTest, OpenClose) {
  EXPECT_FALSE(store_.key_file_);

  EXPECT_FALSE(store_.IsNonEmpty());
  ASSERT_TRUE(store_.Open());
  EXPECT_TRUE(store_.key_file_);
  EXPECT_EQ(1, store_.crypto_.cryptos_.size());
  ASSERT_TRUE(store_.Close());
  EXPECT_FALSE(store_.key_file_);
  FileEnumerator file_enumerator(temp_dir_.path(),
                                 false /* not recursive */,
                                 FileEnumerator::FILES);

  // Verify that the file actually got written with the right name.
  EXPECT_EQ(test_file_.value(), file_enumerator.Next().value());
  FileEnumerator::FindInfo find_info;
  file_enumerator.GetFindInfo(&find_info);

  // Verify that the profile is a regular file, readable and writeable by the
  // owner only.
  EXPECT_EQ(S_IFREG | S_IRUSR | S_IWUSR, find_info.stat.st_mode);

  ASSERT_TRUE(store_.Open());
  EXPECT_TRUE(store_.key_file_);
  ASSERT_TRUE(store_.Close());
  EXPECT_FALSE(store_.key_file_);

  ASSERT_TRUE(store_.Open());
  store_.set_path(FilePath(""));
  ASSERT_FALSE(store_.Close());
  EXPECT_FALSE(store_.key_file_);
}

TEST_F(KeyFileStoreTest, OpenFail) {
  WriteKeyFile("garbage\n");
  EXPECT_FALSE(store_.Open());
  EXPECT_FALSE(store_.key_file_);
}

TEST_F(KeyFileStoreTest, MarkAsCorrupted) {
  EXPECT_FALSE(store_.MarkAsCorrupted());
  EXPECT_FALSE(store_.IsNonEmpty());
  WriteKeyFile("garbage\n");
  EXPECT_TRUE(store_.IsNonEmpty());
  EXPECT_TRUE(file_util::PathExists(store_.path()));
  EXPECT_TRUE(store_.MarkAsCorrupted());
  EXPECT_FALSE(store_.IsNonEmpty());
  EXPECT_FALSE(file_util::PathExists(store_.path()));
  EXPECT_TRUE(
      file_util::PathExists(FilePath(store_.path().value() + ".corrupted")));
}

TEST_F(KeyFileStoreTest, GetGroups) {
  static const char kGroupA[] = "g-a";
  static const char kGroupB[] = "g-b";
  static const char kGroupC[] = "g-c";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "[%s]\n"
                                  "[%s]\n",
                                  kGroupA, kGroupB, kGroupC));
  EXPECT_TRUE(store_.IsNonEmpty());
  ASSERT_TRUE(store_.Open());
  set<string> groups = store_.GetGroups();
  EXPECT_EQ(3, groups.size());
  EXPECT_TRUE(ContainsKey(groups, kGroupA));
  EXPECT_TRUE(ContainsKey(groups, kGroupB));
  EXPECT_TRUE(ContainsKey(groups, kGroupC));
  EXPECT_FALSE(ContainsKey(groups, "g-x"));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, GetGroupsWithKey) {
  static const char kGroupA[] = "g-a";
  static const char kGroupB[] = "g-b";
  static const char kGroupC[] = "g-c";
  static const char kKeyA[] = "k-a";
  static const char kKeyB[] = "k-b";
  static const char kValue[] = "true";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=%s\n"
                                  "[%s]\n"
                                  "%s=%s\n"
                                  "%s=%s\n"
                                  "[%s]\n"
                                  "%s=%s\n",
                                  kGroupA, kKeyA, kValue,
                                  kGroupB, kKeyA, kValue, kKeyB, kValue,
                                  kGroupC, kKeyB, kValue));
  EXPECT_TRUE(store_.IsNonEmpty());
  ASSERT_TRUE(store_.Open());
  set<string> groups_a = store_.GetGroupsWithKey(kKeyA);
  EXPECT_EQ(2, groups_a.size());
  EXPECT_TRUE(ContainsKey(groups_a, kGroupA));
  EXPECT_TRUE(ContainsKey(groups_a, kGroupB));
  set<string> groups_b = store_.GetGroupsWithKey(kKeyB);
  EXPECT_EQ(2, groups_b.size());
  EXPECT_TRUE(ContainsKey(groups_b, kGroupB));
  EXPECT_TRUE(ContainsKey(groups_b, kGroupC));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, ContainsGroup) {
  static const char kGroupA[] = "group-a";
  static const char kGroupB[] = "group-b";
  static const char kGroupC[] = "group-c";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "[%s]\n"
                                  "[%s]\n",
                                  kGroupA, kGroupB, kGroupC));
  ASSERT_TRUE(store_.Open());
  EXPECT_TRUE(store_.ContainsGroup(kGroupA));
  EXPECT_TRUE(store_.ContainsGroup(kGroupB));
  EXPECT_TRUE(store_.ContainsGroup(kGroupC));
  EXPECT_FALSE(store_.ContainsGroup("group-d"));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, DeleteKey) {
  static const char kGroup[] = "the-group";
  static const char kKeyDead[] = "dead";
  static const char kKeyAlive[] = "alive";
  const int kValueAlive = 3;
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=5\n"
                                  "%s=%d\n",
                                  kGroup, kKeyDead, kKeyAlive, kValueAlive));
  ASSERT_TRUE(store_.Open());
  EXPECT_TRUE(store_.DeleteKey(kGroup, kKeyDead));
  EXPECT_TRUE(store_.DeleteKey(kGroup, "random-key"));
  EXPECT_FALSE(store_.DeleteKey("random-group", kKeyAlive));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=%d\n",
                               kGroup, kKeyAlive, kValueAlive),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, DeleteGroup) {
  static const char kGroupA[] = "group-a";
  static const char kGroupB[] = "group-b";
  static const char kGroupC[] = "group-c";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "[%s]\n"
                                  "key-to-be-deleted=true\n"
                                  "[%s]\n",
                                  kGroupA, kGroupB, kGroupC));
  ASSERT_TRUE(store_.Open());
  EXPECT_TRUE(store_.DeleteGroup(kGroupB));
  EXPECT_TRUE(store_.DeleteGroup("group-d"));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "\n"
                               "[%s]\n",
                               kGroupA, kGroupC),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, GetString) {
  static const char kGroup[] = "something";
  static const char kKey[] = "foo";
  static const char kValue[] = "bar";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=%s\n",
                                  kGroup, kKey, kValue));
  ASSERT_TRUE(store_.Open());
  string value;
  EXPECT_TRUE(store_.GetString(kGroup, kKey, &value));
  EXPECT_EQ(kValue, value);
  EXPECT_FALSE(store_.GetString("something-else", kKey, &value));
  EXPECT_FALSE(store_.GetString(kGroup, "bar", &value));
  EXPECT_TRUE(store_.GetString(kGroup, kKey, NULL));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, SetString) {
  static const char kGroup[] = "string-group";
  static const char kKey1[] = "test-string";
  static const char kValue1[] = "foo";
  static const char kKey2[] = "empty-string";
  static const char kValue2[] = "";
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetString(kGroup, kKey1, kValue1));
  ASSERT_TRUE(store_.SetString(kGroup, kKey2, kValue2));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=%s\n"
                               "%s=%s\n",
                               kGroup, kKey1, kValue1, kKey2, kValue2),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, GetBool) {
  static const char kGroup[] = "boo";
  static const char kKeyTrue[] = "foo";
  static const char kKeyFalse[] = "bar";
  static const char kKeyBad[] = "zoo";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=true\n"
                                  "%s=false\n"
                                  "%s=moo\n",
                                  kGroup, kKeyTrue, kKeyFalse, kKeyBad));
  ASSERT_TRUE(store_.Open());
  {
    bool value = true;
    EXPECT_TRUE(store_.GetBool(kGroup, kKeyFalse, &value));
    EXPECT_FALSE(value);
  }
  {
    bool value = false;
    EXPECT_TRUE(store_.GetBool(kGroup, kKeyTrue, &value));
    EXPECT_TRUE(value);
  }
  {
    bool value;
    EXPECT_FALSE(store_.GetBool(kGroup, kKeyBad, &value));
    EXPECT_FALSE(store_.GetBool(kGroup, "unknown", &value));
    EXPECT_FALSE(store_.GetBool("unknown", kKeyTrue, &value));
  }
  EXPECT_TRUE(store_.GetBool(kGroup, kKeyFalse, NULL));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, SetBool) {
  static const char kGroup[] = "bool-group";
  static const char kKeyTrue[] = "test-true-bool";
  static const char kKeyFalse[] = "test-false-bool";
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetBool(kGroup, kKeyTrue, true));
  ASSERT_TRUE(store_.SetBool(kGroup, kKeyFalse, false));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=true\n"
                               "%s=false\n",
                               kGroup, kKeyTrue, kKeyFalse),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, GetInt) {
  static const char kGroup[] = "numbers";
  static const char kKeyPos[] = "pos";
  static const char kKeyNeg[] = "neg";
  static const char kKeyBad[] = "bad";
  const int kValuePos = 50;
  const int kValueNeg = -20;
  static const char kValueBad[] = "nan";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=%d\n"
                                  "%s=%d\n"
                                  "%s=%s\n",
                                  kGroup,
                                  kKeyPos, kValuePos,
                                  kKeyNeg, kValueNeg,
                                  kKeyBad, kValueBad));
  ASSERT_TRUE(store_.Open());
  {
    int value = 0;
    EXPECT_TRUE(store_.GetInt(kGroup, kKeyNeg, &value));
    EXPECT_EQ(kValueNeg, value);
  }
  {
    int value = 0;
    EXPECT_TRUE(store_.GetInt(kGroup, kKeyPos, &value));
    EXPECT_EQ(kValuePos, value);
  }
  {
    int value;
    EXPECT_FALSE(store_.GetInt(kGroup, kKeyBad, &value));
    EXPECT_FALSE(store_.GetInt(kGroup, "invalid", &value));
    EXPECT_FALSE(store_.GetInt("invalid", kKeyPos, &value));
  }
  EXPECT_TRUE(store_.GetInt(kGroup, kKeyPos, NULL));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, SetInt) {
  static const char kGroup[] = "int-group";
  static const char kKey1[] = "test-int";
  static const char kKey2[] = "test-negative";
  const int kValue1 = 5;
  const int kValue2 = -10;
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetInt(kGroup, kKey1, kValue1));
  ASSERT_TRUE(store_.SetInt(kGroup, kKey2, kValue2));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=%d\n"
                               "%s=%d\n",
                               kGroup, kKey1, kValue1, kKey2, kValue2),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, GetUint64) {
  static const char kGroup[] = "numbers";
  static const char kKeyGood[] = "good";
  static const char kKeyBad[] = "bad";
  const uint64 kValueGood = 0xFEDCBA9876543210LL;
  static const char kValueBad[] = "nan";
  // Use base::Uint64ToString() instead of using something like "%llu"
  // (not correct for native 64 bit architectures) or PRIu64 (does not
  // work correctly using cros_workon_make due to include intricacies).
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=%s\n"
                                  "%s=%s\n",
                                  kGroup,
                                  kKeyGood,
                                  base::Uint64ToString(kValueGood).c_str(),
                                  kKeyBad, kValueBad));
  ASSERT_TRUE(store_.Open());
  {
    uint64 value = 0;
    EXPECT_TRUE(store_.GetUint64(kGroup, kKeyGood, &value));
    EXPECT_EQ(kValueGood, value);
  }
  {
    uint64 value;
    EXPECT_FALSE(store_.GetUint64(kGroup, kKeyBad, &value));
    EXPECT_FALSE(store_.GetUint64(kGroup, "invalid", &value));
    EXPECT_FALSE(store_.GetUint64("invalid", kKeyGood, &value));
  }
  EXPECT_TRUE(store_.GetUint64(kGroup, kKeyGood, NULL));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, SetUint64) {
  static const char kGroup[] = "int-group";
  static const char kKey[] = "test-int";
  const uint64 kValue = 0xFEDCBA9876543210LL;
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetUint64(kGroup, kKey, kValue));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=%s\n",
                               kGroup, kKey,
                               base::Uint64ToString(kValue).c_str()),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, GetStringList) {
  static const char kGroup[] = "string-lists";
  static const char kKeyEmpty[] = "empty";
  static const char kKeyEmptyValue[] = "empty-value";
  static const char kKeyValueEmpty[] = "value-empty";
  static const char kKeyValueEmptyValue[] = "value-empty-value";
  static const char kKeyValues[] = "values";
  static const char kValue[] = "value";
  static const char kValue2[] = "value2";
  static const char kValue3[] = "value3";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=\n"
                                  "%s=;%s\n"
                                  "%s=%s;;\n"
                                  "%s=%s;;%s\n"
                                  "%s=%s;%s;%s\n",
                                  kGroup,
                                  kKeyEmpty,
                                  kKeyEmptyValue, kValue,
                                  kKeyValueEmpty, kValue,
                                  kKeyValueEmptyValue, kValue, kValue2,
                                  kKeyValues, kValue, kValue2, kValue3));
  ASSERT_TRUE(store_.Open());

  vector<string> value;

  EXPECT_TRUE(store_.GetStringList(kGroup, kKeyValues, &value));
  ASSERT_EQ(3, value.size());
  EXPECT_EQ(kValue, value[0]);
  EXPECT_EQ(kValue2, value[1]);
  EXPECT_EQ(kValue3, value[2]);

  EXPECT_TRUE(store_.GetStringList(kGroup, kKeyEmptyValue, &value));
  ASSERT_EQ(2, value.size());
  EXPECT_EQ("", value[0]);
  EXPECT_EQ(kValue, value[1]);

  EXPECT_TRUE(store_.GetStringList(kGroup, kKeyValueEmpty, &value));
  ASSERT_EQ(2, value.size());
  EXPECT_EQ(kValue, value[0]);
  EXPECT_EQ("", value[1]);

  EXPECT_TRUE(store_.GetStringList(kGroup, kKeyEmpty, &value));
  ASSERT_EQ(0, value.size());

  EXPECT_TRUE(store_.GetStringList(kGroup, kKeyValueEmptyValue, &value));
  ASSERT_EQ(3, value.size());
  EXPECT_EQ(kValue, value[0]);
  EXPECT_EQ("", value[1]);
  EXPECT_EQ(kValue2, value[2]);

  EXPECT_FALSE(store_.GetStringList("unknown-string-lists", kKeyEmpty, &value));
  EXPECT_FALSE(store_.GetStringList(kGroup, "some-key", &value));
  EXPECT_TRUE(store_.GetStringList(kGroup, kKeyValues, NULL));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, SetStringList) {
  static const char kGroup[] = "strings";
  static const char kKeyEmpty[] = "e";
  static const char kKeyEmptyValue[] = "ev";
  static const char kKeyValueEmpty[] = "ve";
  static const char kKeyValueEmptyValue[] = "vev";
  static const char kKeyValues[] = "v";
  static const char kValue[] = "abc";
  static const char kValue2[] = "pqr";
  static const char kValue3[] = "xyz";
  ASSERT_TRUE(store_.Open());
  {
    vector<string> value;
    ASSERT_TRUE(store_.SetStringList(kGroup, kKeyEmpty, value));
  }
  {
    vector<string> value;
    value.push_back("");
    value.push_back(kValue);
    ASSERT_TRUE(store_.SetStringList(kGroup, kKeyEmptyValue, value));
  }
  {
    vector<string> value;
    value.push_back(kValue);
    value.push_back("");
    ASSERT_TRUE(store_.SetStringList(kGroup, kKeyValueEmpty, value));
  }
  {
    vector<string> value;
    value.push_back(kValue);
    value.push_back("");
    value.push_back(kValue2);
    ASSERT_TRUE(store_.SetStringList(kGroup, kKeyValueEmptyValue, value));
  }
  {
    vector<string> value;
    value.push_back(kValue);
    value.push_back(kValue2);
    value.push_back(kValue3);
    ASSERT_TRUE(store_.SetStringList(kGroup, kKeyValues, value));
  }
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=\n"
                               "%s=;%s;\n"
                               "%s=%s;;\n"
                               "%s=%s;;%s;\n"
                               "%s=%s;%s;%s;\n",
                               kGroup,
                               kKeyEmpty,
                               kKeyEmptyValue, kValue,
                               kKeyValueEmpty, kValue,
                               kKeyValueEmptyValue, kValue, kValue2,
                               kKeyValues, kValue, kValue2, kValue3),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, GetCryptedString) {
  static const char kGroup[] = "crypto-group";
  static const char kKey[] = "secret";
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=%s\n",
                                  kGroup, kKey, kROT47Text));
  ASSERT_TRUE(store_.Open());
  string value;
  EXPECT_TRUE(store_.GetCryptedString(kGroup, kKey, &value));
  EXPECT_EQ(kPlainText, value);
  EXPECT_FALSE(store_.GetCryptedString("something-else", kKey, &value));
  EXPECT_FALSE(store_.GetCryptedString(kGroup, "non-secret", &value));
  EXPECT_TRUE(store_.GetCryptedString(kGroup, kKey, NULL));
  ASSERT_TRUE(store_.Close());
}

TEST_F(KeyFileStoreTest, SetCryptedString) {
  static const char kGroup[] = "crypted-string-group";
  static const char kKey[] = "test-string";
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetCryptedString(kGroup, kKey, kPlainText));
  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=%s\n",
                               kGroup, kKey, kROT47Text),
            ReadKeyFile());
}

TEST_F(KeyFileStoreTest, PersistAcrossClose) {
  static const char kGroup[] = "string-group";
  static const char kKey1[] = "test-string";
  static const char kValue1[] = "foo";
  static const char kKey2[] = "empty-string";
  static const char kValue2[] = "";
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetString(kGroup, kKey1, kValue1));
  ASSERT_TRUE(store_.Close());
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetString(kGroup, kKey2, kValue2));
  string value;
  ASSERT_TRUE(store_.GetString(kGroup, kKey1, &value));
  ASSERT_EQ(kValue1, value);
  ASSERT_TRUE(store_.GetString(kGroup, kKey2, &value));
  ASSERT_EQ(kValue2, value);
  ASSERT_TRUE(store_.Close());
}

bool KeyFileStoreTest::OpenCheckClose(const string &group,
                                      const string &key,
                                      const string &expected_value) {
  KeyFileStore store(&glib_);
  store.set_path(test_file_);
  EXPECT_TRUE(store.Open());
  string value;
  bool could_get = store.GetString(group, key, &value);
  store.set_path(FilePath(""));  // Don't persist to disk.
  store.Close();
  return could_get && expected_value == value;
}

TEST_F(KeyFileStoreTest, Flush) {
  static const char kGroup[] = "string-group";
  static const char kKey1[] = "test-string";
  static const char kValue1[] = "foo";
  static const char kKey2[] = "empty-string";
  static const char kValue2[] = "";
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetString(kGroup, kKey1, kValue1));
  ASSERT_TRUE(store_.Flush());
  ASSERT_TRUE(OpenCheckClose(kGroup, kKey1, kValue1));

  ASSERT_TRUE(store_.SetString(kGroup, kKey2, kValue2));
  ASSERT_TRUE(store_.Flush());
  ASSERT_TRUE(OpenCheckClose(kGroup, kKey2, kValue2));

  EXPECT_TRUE(store_.DeleteKey(kGroup, kKey1));
  ASSERT_TRUE(store_.Flush());
  ASSERT_FALSE(OpenCheckClose(kGroup, kKey1, kValue1));
}

TEST_F(KeyFileStoreTest, EmptyFile) {
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.Close());
  EXPECT_FALSE(store_.IsNonEmpty());
}

TEST_F(KeyFileStoreTest, SetHeader) {
  ASSERT_TRUE(store_.Open());
  ASSERT_TRUE(store_.SetHeader("this is a test"));
  ASSERT_TRUE(store_.Close());
  EXPECT_TRUE(store_.IsNonEmpty());
  ASSERT_TRUE(store_.Open());
}

TEST_F(KeyFileStoreTest, Combo) {
  static const char kGroupA[] = "square";
  static const char kGroupB[] = "circle";
  static const char kGroupC[] = "triangle";
  static const char kGroupX[] = "pentagon";
  static const char kKeyString[] = "color";
  static const char kKeyStringList[] = "alternative-colors";
  static const char kKeyInt[] = "area";
  static const char kKeyBool[] = "visible";
  static const char kValueStringA[] = "blue";
  static const char kValueStringB[] = "red";
  static const char kValueStringC[] = "yellow";
  static const char kValueStringCNew[] = "purple";
  const int kValueIntA = 5;
  const int kValueIntB = 10;
  const int kValueIntBNew = 333;
  WriteKeyFile(base::StringPrintf("[%s]\n"
                                  "%s=%s\n"
                                  "%s=%s;%s\n"
                                  "%s=%d\n"
                                  "[%s]\n"
                                  "%s=%s\n"
                                  "%s=%s;%s\n"
                                  "%s=%d\n"
                                  "%s=true\n"
                                  "[%s]\n"
                                  "%s=%s\n"
                                  "%s=false\n",
                                  kGroupA,
                                  kKeyString, kValueStringA,
                                  kKeyStringList, kValueStringB, kValueStringC,
                                  kKeyInt, kValueIntA,
                                  kGroupB,
                                  kKeyString, kValueStringB,
                                  kKeyStringList, kValueStringA, kValueStringC,
                                  kKeyInt, kValueIntB,
                                  kKeyBool,
                                  kGroupC,
                                  kKeyString, kValueStringC,
                                  kKeyBool));
  ASSERT_TRUE(store_.Open());

  EXPECT_TRUE(store_.ContainsGroup(kGroupA));
  EXPECT_TRUE(store_.ContainsGroup(kGroupB));
  EXPECT_TRUE(store_.ContainsGroup(kGroupC));
  EXPECT_FALSE(store_.ContainsGroup(kGroupX));

  set<string> groups = store_.GetGroups();
  EXPECT_EQ(3, groups.size());
  EXPECT_TRUE(ContainsKey(groups, kGroupA));
  EXPECT_TRUE(ContainsKey(groups, kGroupB));
  EXPECT_TRUE(ContainsKey(groups, kGroupC));
  EXPECT_FALSE(ContainsKey(groups, kGroupX));

  {
    string value;
    EXPECT_TRUE(store_.GetString(kGroupB, kKeyString, &value));
    EXPECT_EQ(kValueStringB, value);
    EXPECT_TRUE(store_.GetString(kGroupA, kKeyString, &value));
    EXPECT_EQ(kValueStringA, value);
    EXPECT_TRUE(store_.GetString(kGroupC, kKeyString, &value));
    EXPECT_EQ(kValueStringC, value);
  }
  {
    vector<string> value;
    EXPECT_TRUE(store_.GetStringList(kGroupB, kKeyStringList, &value));
    ASSERT_EQ(2, value.size());
    EXPECT_EQ(kValueStringA, value[0]);
    EXPECT_EQ(kValueStringC, value[1]);
    EXPECT_TRUE(store_.GetStringList(kGroupA, kKeyStringList, &value));
    ASSERT_EQ(2, value.size());
    EXPECT_EQ(kValueStringB, value[0]);
    EXPECT_EQ(kValueStringC, value[1]);
    EXPECT_FALSE(store_.GetStringList(kGroupC, kKeyStringList, &value));
  }
  {
    int value = 0;
    EXPECT_TRUE(store_.GetInt(kGroupB, kKeyInt, &value));
    EXPECT_EQ(kValueIntB, value);
    EXPECT_TRUE(store_.GetInt(kGroupA, kKeyInt, &value));
    EXPECT_EQ(kValueIntA, value);
    EXPECT_FALSE(store_.GetInt(kGroupC, kKeyInt, &value));
  }
  {
    bool value = false;
    EXPECT_TRUE(store_.GetBool(kGroupB, kKeyBool, &value));
    EXPECT_TRUE(value);
    EXPECT_TRUE(store_.GetBool(kGroupC, kKeyBool, &value));
    EXPECT_FALSE(value);
    EXPECT_FALSE(store_.GetBool(kGroupA, kKeyBool, &value));
  }

  EXPECT_TRUE(store_.DeleteGroup(kGroupA));
  EXPECT_TRUE(store_.DeleteGroup(kGroupA));

  EXPECT_FALSE(store_.ContainsGroup(kGroupA));
  EXPECT_TRUE(store_.ContainsGroup(kGroupB));
  EXPECT_TRUE(store_.ContainsGroup(kGroupC));

  groups = store_.GetGroups();
  EXPECT_EQ(2, groups.size());
  EXPECT_FALSE(ContainsKey(groups, kGroupA));
  EXPECT_TRUE(ContainsKey(groups, kGroupB));
  EXPECT_TRUE(ContainsKey(groups, kGroupC));

  EXPECT_TRUE(store_.SetBool(kGroupB, kKeyBool, false));
  EXPECT_TRUE(store_.SetInt(kGroupB, kKeyInt, kValueIntBNew));
  EXPECT_TRUE(store_.SetString(kGroupC, kKeyString, kValueStringCNew));
  store_.SetStringList(kGroupB,
                       kKeyStringList,
                       vector<string>(1, kValueStringB));

  EXPECT_TRUE(store_.DeleteKey(kGroupB, kKeyString));
  EXPECT_TRUE(store_.DeleteKey(kGroupB, kKeyString));

  {
    string value;
    EXPECT_FALSE(store_.GetString(kGroupB, kKeyString, &value));
    EXPECT_FALSE(store_.GetString(kGroupA, kKeyString, &value));
    EXPECT_TRUE(store_.GetString(kGroupC, kKeyString, &value));
    EXPECT_EQ(kValueStringCNew, value);
  }
  {
    vector<string> value;
    EXPECT_TRUE(store_.GetStringList(kGroupB, kKeyStringList, &value));
    ASSERT_EQ(1, value.size());
    EXPECT_EQ(kValueStringB, value[0]);
    EXPECT_FALSE(store_.GetStringList(kGroupA, kKeyStringList, &value));
    EXPECT_FALSE(store_.GetStringList(kGroupC, kKeyStringList, &value));
  }
  {
    int value = 0;
    EXPECT_TRUE(store_.GetInt(kGroupB, kKeyInt, &value));
    EXPECT_EQ(kValueIntBNew, value);
    EXPECT_FALSE(store_.GetInt(kGroupA, kKeyInt, &value));
    EXPECT_FALSE(store_.GetInt(kGroupC, kKeyInt, &value));
  }
  {
    bool value = false;
    EXPECT_TRUE(store_.GetBool(kGroupB, kKeyBool, &value));
    EXPECT_FALSE(value);
    EXPECT_TRUE(store_.GetBool(kGroupC, kKeyBool, &value));
    EXPECT_FALSE(value);
    EXPECT_FALSE(store_.GetBool(kGroupA, kKeyBool, &value));
  }

  ASSERT_TRUE(store_.Close());
  EXPECT_EQ(base::StringPrintf("\n"
                               "[%s]\n"
                               "%s=%s;\n"
                               "%s=%d\n"
                               "%s=false\n"
                               "\n"
                               "[%s]\n"
                               "%s=%s\n"
                               "%s=false\n",
                               kGroupB,
                               kKeyStringList, kValueStringB,
                               kKeyInt, kValueIntBNew,
                               kKeyBool,
                               kGroupC,
                               kKeyString, kValueStringCNew,
                               kKeyBool),
            ReadKeyFile());
}

}  // namespace shill

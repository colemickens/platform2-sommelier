// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/json_store.h"

#include <array>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <base/strings/string_util.h>
#include <gtest/gtest.h>

#include "shill/mock_log.h"

using std::array;
using std::pair;
using std::set;
using std::string;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Test;

namespace shill {

class JsonStoreTest : public Test {
 public:
  JsonStoreTest()
      : kStringWithEmbeddedNulls({0, 'a', 0, 'z'}),
        kNonUtf8String("ab\xc0") {}

  virtual void SetUp() {
    ScopeLogger::GetInstance()->EnableScopesByName("+storage");
    ASSERT_FALSE(base::IsStringUTF8(kNonUtf8String));
    EXPECT_CALL(log_, Log(_, _, _)).Times(AnyNumber());
  }

  virtual void TearDown() {
    ScopeLogger::GetInstance()->EnableScopesByName("-storage");
    ScopeLogger::GetInstance()->set_verbose_level(0);
  }

 protected:
  void SetVerboseLevel(int new_level);

  const string kStringWithEmbeddedNulls;
  const string kNonUtf8String;
  JsonStore store_;
  ScopedMockLog log_;
};

void JsonStoreTest::SetVerboseLevel(int new_level) {
  ScopeLogger::GetInstance()->set_verbose_level(new_level);
}

// In memory operations: basic storage and retrieval.
TEST_F(JsonStoreTest, StringsCanBeStoredInMemory) {
  const array<string, 5> our_values{
    {"", "hello", "world\n", kStringWithEmbeddedNulls, kNonUtf8String}};
  for (const auto& our_value : our_values) {
    string value_from_store;
    EXPECT_TRUE(store_.SetString("group_a", "knob_1", our_value));
    EXPECT_TRUE(store_.GetString("group_a", "knob_1", &value_from_store));
    EXPECT_EQ(our_value, value_from_store);
  }
}

TEST_F(JsonStoreTest, BoolsCanBeStoredInMemory) {
  const array<bool, 2> our_values{{false, true}};
  for (const auto& our_value : our_values) {
    bool value_from_store;
    EXPECT_TRUE(store_.SetBool("group_a", "knob_1", our_value));
    EXPECT_TRUE(store_.GetBool("group_a", "knob_1", &value_from_store));
    EXPECT_EQ(our_value, value_from_store);
  }
}

TEST_F(JsonStoreTest, IntsCanBeStoredInMemory) {
  const array<int, 3> our_values{{
      std::numeric_limits<int>::min(), 0, std::numeric_limits<int>::max()}};
  for (const auto& our_value : our_values) {
    int value_from_store;
    EXPECT_TRUE(store_.SetInt("group_a", "knob_1", our_value));
    EXPECT_TRUE(store_.GetInt("group_a", "knob_1", &value_from_store));
    EXPECT_EQ(our_value, value_from_store);
  }
}

TEST_F(JsonStoreTest, Uint64sCanBeStoredInMemory) {
  const array<uint64_t, 3> our_values{{
      std::numeric_limits<uint64_t>::min(),
      0,
      std::numeric_limits<uint64_t>::max()}};
  for (const auto& our_value : our_values) {
    uint64_t value_from_store;
    EXPECT_TRUE(store_.SetUint64("group_a", "knob_1", our_value));
    EXPECT_TRUE(store_.GetUint64("group_a", "knob_1", &value_from_store));
    EXPECT_EQ(our_value, value_from_store);
  }
}

TEST_F(JsonStoreTest, StringListsCanBeStoredInMemory) {
  const array<vector<string>, 7> our_values{{
      vector<string>{},
      vector<string>{""},
      vector<string>{"a"},
      vector<string>{"", "a"},
      vector<string>{"a", ""},
      vector<string>{"", "a", ""},
      vector<string>{"a", "b", "c", kStringWithEmbeddedNulls, kNonUtf8String}}};
  for (const auto& our_value : our_values) {
    vector<string> value_from_store;
    EXPECT_TRUE(store_.SetStringList("group_a", "knob_1", our_value));
    EXPECT_TRUE(store_.GetStringList("group_a", "knob_1", &value_from_store));
    EXPECT_EQ(our_value, value_from_store);
  }
}

TEST_F(JsonStoreTest, CryptedStringsCanBeStoredInMemory) {
  const array<string, 5> our_values{{
      string(), string("some stuff"), kStringWithEmbeddedNulls, kNonUtf8String
  }};
  for (const auto& our_value : our_values) {
    string value_from_store;
    EXPECT_TRUE(store_.SetCryptedString("group_a", "knob_1", our_value));
    EXPECT_TRUE(
        store_.GetCryptedString("group_a", "knob_1", &value_from_store));
    EXPECT_EQ(our_value, value_from_store);
  }
}

TEST_F(JsonStoreTest, RawValuesOfCryptedStringsDifferFromOriginalValues) {
  const array<string, 3> our_values{{
      string("simple string"), kStringWithEmbeddedNulls, kNonUtf8String
  }};
  for (const auto& our_value : our_values) {
    string raw_value_from_store;
    EXPECT_TRUE(store_.SetCryptedString("group_a", "knob_1", our_value));
    EXPECT_TRUE(store_.GetString("group_a", "knob_1", &raw_value_from_store));
    EXPECT_NE(our_value, raw_value_from_store);
  }
}

TEST_F(JsonStoreTest, DifferentGroupsCanHaveDifferentValuesForSameKey) {
  store_.SetString("group_a", "knob_1", "value_1");
  store_.SetString("group_b", "knob_1", "value_2");

  string value_from_store;
  EXPECT_TRUE(store_.GetString("group_a", "knob_1", &value_from_store));
  EXPECT_EQ("value_1", value_from_store);
  EXPECT_TRUE(store_.GetString("group_b", "knob_1", &value_from_store));
  EXPECT_EQ("value_2", value_from_store);
}

// In memory operations: presence checking.
TEST_F(JsonStoreTest, CanUseNullptrToCheckPresenceOfKey) {
  SetVerboseLevel(10);

  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find group"))).Times(6);
  EXPECT_FALSE(store_.GetString("group_a", "string_knob", nullptr));
  EXPECT_FALSE(store_.GetBool("group_a", "bool_knob", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "int_knob", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "uint64_knob", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "string_list_knob", nullptr));
  EXPECT_FALSE(
      store_.GetCryptedString("group_a", "crypted_string_knob", nullptr));

  ASSERT_TRUE(store_.SetString("group_a", "random_knob", "random value"));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find property"))).Times(6);
  EXPECT_FALSE(store_.GetString("group_a", "string_knob", nullptr));
  EXPECT_FALSE(store_.GetBool("group_a", "bool_knob", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "int_knob", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "uint64_knob", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "string_list_knob", nullptr));
  EXPECT_FALSE(
      store_.GetCryptedString("group_a", "crypted_string_knob", nullptr));

  ASSERT_TRUE(store_.SetString("group_a", "string_knob", "stuff goes here"));
  ASSERT_TRUE(store_.SetBool("group_a", "bool_knob", true));
  ASSERT_TRUE(store_.SetInt("group_a", "int_knob", -1));
  ASSERT_TRUE(store_.SetUint64("group_a", "uint64_knob", 1));
  ASSERT_TRUE(store_.SetStringList(
      "group_a", "string_list_knob", vector<string>{{"hello"}}));
  ASSERT_TRUE(
      store_.SetCryptedString("group_a", "crypted_string_knob", "s3kr!t"));

  EXPECT_TRUE(store_.GetString("group_a", "string_knob", nullptr));
  EXPECT_TRUE(store_.GetBool("group_a", "bool_knob", nullptr));
  EXPECT_TRUE(store_.GetInt("group_a", "int_knob", nullptr));
  EXPECT_TRUE(store_.GetUint64("group_a", "uint64_knob", nullptr));
  EXPECT_TRUE(store_.GetStringList("group_a", "string_list_knob", nullptr));
  EXPECT_TRUE(
      store_.GetCryptedString("group_a", "crypted_string_knob", nullptr));
}

// In memory operations: access to missing elements.
TEST_F(JsonStoreTest, GetFromEmptyStoreFails) {
  bool value_from_store;
  SetVerboseLevel(10);
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find group")));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", &value_from_store));
}

TEST_F(JsonStoreTest, GetFromNonexistentGroupAndKeyFails) {
  bool value_from_store;
  SetVerboseLevel(10);
  EXPECT_TRUE(store_.SetBool("group_a", "knob_1", true));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find group")));
  EXPECT_FALSE(store_.GetBool("group_b", "knob_1", &value_from_store));
}

TEST_F(JsonStoreTest, GetOfNonexistentPropertyFails) {
  bool value_from_store;
  SetVerboseLevel(10);
  EXPECT_TRUE(store_.SetBool("group_a", "knob_1", true));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find property")));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_2", &value_from_store));
}

TEST_F(JsonStoreTest, GetOfPropertyFromWrongGroupFails) {
  bool value_from_store;
  SetVerboseLevel(10);
  EXPECT_TRUE(store_.SetBool("group_a", "knob_1", true));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find group")));
  EXPECT_FALSE(store_.GetBool("group_b", "knob_1", &value_from_store));
}

TEST_F(JsonStoreTest, GetDoesNotMatchOnValue) {
  string value_from_store;
  SetVerboseLevel(10);
  EXPECT_TRUE(store_.SetString("group_a", "knob_1", "value_1"));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find property")));
  EXPECT_FALSE(store_.GetString("group_a", "value_1", &value_from_store));
}

// In memory operations: type conversions on read.
TEST_F(JsonStoreTest, ConversionFromStringIsProhibited) {
  EXPECT_CALL(
      log_,
      Log(logging::LOG_ERROR, _,
          ContainsRegex("Can not read \\|.+\\| from \\|.+\\|"))).Times(4);
  EXPECT_TRUE(store_.SetString("group_a", "knob_1", "stuff goes here"));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "knob_1", nullptr));
  // We deliberately omit checking store_.GetCryptedString(). While
  // this "works" right now, it's not something we're committed to.
}

TEST_F(JsonStoreTest, ConversionFromBoolIsProhibited) {
  EXPECT_CALL(
      log_,
      Log(logging::LOG_ERROR, _,
          ContainsRegex("Can not read \\|.+\\| from \\|.+\\|"))).Times(5);
  EXPECT_TRUE(store_.SetBool("group_a", "knob_1", true));
  EXPECT_FALSE(store_.GetString("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetCryptedString("group_a", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, ConversionFromIntIsProhibited) {
  EXPECT_CALL(
      log_,
      Log(logging::LOG_ERROR, _,
          ContainsRegex("Can not read \\|.+\\| from \\|.+\\|"))).Times(5);
  EXPECT_TRUE(store_.SetInt("group_a", "knob_1", -1));
  EXPECT_FALSE(store_.GetString("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetCryptedString("group_a", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, ConversionFromUint64IsProhibited) {
  EXPECT_CALL(
      log_,
      Log(logging::LOG_ERROR, _,
          ContainsRegex("Can not read \\|.+\\| from \\|.+\\|"))).Times(5);
  EXPECT_TRUE(store_.SetUint64("group_a", "knob_1", 1));
  EXPECT_FALSE(store_.GetString("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetCryptedString("group_a", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, ConversionFromStringListIsProhibited) {
  EXPECT_CALL(
      log_,
      Log(logging::LOG_ERROR, _,
          ContainsRegex("Can not read \\|.+\\| from \\|.+\\|"))).Times(5);
  EXPECT_TRUE(store_.SetStringList(
      "group_a", "knob_1", vector<string>{{"hello"}}));
  EXPECT_FALSE(store_.GetString("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetCryptedString("group_a", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, ConversionFromCryptedStringIsProhibited) {
  EXPECT_CALL(
      log_,
      Log(logging::LOG_ERROR, _,
          ContainsRegex("Can not read \\|.+\\| from \\|.+\\|"))).Times(4);
  EXPECT_TRUE(store_.SetCryptedString("group_a", "knob_1", "s3kr!t"));
  // We deliberately omit checking store_.GetString(). While this
  // "works" right now, it's not something we're committed to.
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetInt("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetUint64("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetStringList("group_a", "knob_1", nullptr));
}

// In memory operations: key deletion.
TEST_F(JsonStoreTest, DeleteKeyDeletesExistingKey) {
  SetVerboseLevel(10);
  store_.SetBool("group_a", "knob_1", bool());
  EXPECT_TRUE(store_.DeleteKey("group_a", "knob_1"));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find property")));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, DeleteKeyDeletesOnlySpecifiedKey) {
  store_.SetBool("group_a", "knob_1", bool());
  store_.SetBool("group_a", "knob_2", bool());
  EXPECT_TRUE(store_.DeleteKey("group_a", "knob_1"));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_TRUE(store_.GetBool("group_a", "knob_2", nullptr));
}

TEST_F(JsonStoreTest, DeleteKeySucceedsOnMissingKey) {
  store_.SetBool("group_a", "knob_1", bool());
  EXPECT_TRUE(store_.DeleteKey("group_a", "knob_2"));
  EXPECT_TRUE(store_.GetBool("group_a", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, DeleteKeyFailsWhenGivenWrongGroup) {
  SetVerboseLevel(10);
  store_.SetBool("group_a", "knob_1", bool());
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find group")));
  EXPECT_FALSE(store_.DeleteKey("group_b", "knob_1"));
  EXPECT_TRUE(store_.GetBool("group_a", "knob_1", nullptr));
}

// In memory operations: group operations.
TEST_F(JsonStoreTest, EmptyStoreReturnsNoGroups) {
  EXPECT_EQ(set<string>(), store_.GetGroups());
  EXPECT_EQ(set<string>(), store_.GetGroupsWithKey("knob_1"));
  EXPECT_EQ(set<string>(), store_.GetGroupsWithProperties(KeyValueStore()));
}

TEST_F(JsonStoreTest, GetGroupsReturnsAllGroups) {
  store_.SetBool("group_a", "knob_1", bool());
  store_.SetBool("group_b", "knob_1", bool());
  EXPECT_EQ(set<string>({"group_a", "group_b"}), store_.GetGroups());
}

TEST_F(JsonStoreTest, GetGroupsWithKeyReturnsAllMatchingGroups) {
  store_.SetBool("group_a", "knob_1", bool());
  store_.SetBool("group_b", "knob_1", bool());
  EXPECT_EQ(set<string>({"group_a", "group_b"}),
            store_.GetGroupsWithKey("knob_1"));
}

TEST_F(JsonStoreTest, GetGroupsWithKeyReturnsOnlyMatchingGroups) {
  store_.SetBool("group_a", "knob_1", bool());
  store_.SetBool("group_b", "knob_2", bool());
  EXPECT_EQ(set<string>({"group_a"}), store_.GetGroupsWithKey("knob_1"));
}

TEST_F(JsonStoreTest, GetGroupsWithPropertiesReturnsAllMatchingGroups) {
  store_.SetBool("group_a", "knob_1", true);
  store_.SetBool("group_b", "knob_1", true);

  KeyValueStore required_properties;
  required_properties.SetBool("knob_1", true);
  EXPECT_EQ(set<string>({"group_a", "group_b"}),
            store_.GetGroupsWithProperties(required_properties));
}

TEST_F(JsonStoreTest, GetGroupsWithPropertiesReturnsOnlyMatchingGroups) {
  store_.SetBool("group_a", "knob_1", true);
  store_.SetBool("group_b", "knob_1", false);

  KeyValueStore required_properties;
  required_properties.SetBool("knob_1", true);
  EXPECT_EQ(set<string>({"group_a"}),
            store_.GetGroupsWithProperties(required_properties));
}

TEST_F(JsonStoreTest, GetGroupsWithPropertiesCanMatchOnMultipleProperties) {
  store_.SetBool("group_a", "knob_1", true);
  store_.SetBool("group_a", "knob_2", true);
  store_.SetBool("group_b", "knob_1", true);
  store_.SetBool("group_b", "knob_2", false);

  KeyValueStore required_properties;
  required_properties.SetBool("knob_1", true);
  required_properties.SetBool("knob_2", true);
  EXPECT_EQ(set<string>({"group_a"}),
            store_.GetGroupsWithProperties(required_properties));
}

TEST_F(JsonStoreTest, GetGroupsWithPropertiesChecksValuesForBoolIntAndString) {
  // Documentation in StoreInterface says GetGroupsWithProperties
  // checks only Bool, Int, and String properties. For now, we interpret
  // that permissively. i.e., checking other types is not guaranteed one
  // way or the other.
  //
  // Said differently: we test that that Bool, Int, and String are
  // supported. But we don't test that other types are ignored. (In
  // fact, JsonStore supports filtering on uint64 and StringList as
  // well. JsonStore does not, however, support filtering on
  // CryptedStrings.)
  //
  // This should be fine, as StoreInterface clients currently only use
  // String value filtering.
  const chromeos::VariantDictionary exact_matcher({
      {"knob_1", string("good-string")},
      {"knob_2", bool{true}},
      {"knob_3", int{1}},
    });
  store_.SetString("group_a", "knob_1", "good-string");
  store_.SetBool("group_a", "knob_2", true);
  store_.SetInt("group_a", "knob_3", 1);

  {
    KeyValueStore correct_properties;
    KeyValueStore::ConvertFromVariantDictionary(
        exact_matcher, &correct_properties);
    EXPECT_EQ(set<string>({"group_a"}),
              store_.GetGroupsWithProperties(correct_properties));
  }

  const vector<pair<string, chromeos::Any>> bad_matchers({
      {"knob_1", string("bad-string")},
      {"knob_2", bool{false}},
      {"knob_3", int{2}},
    });
  for (const auto& match_key_and_value : bad_matchers) {
    const auto& match_key = match_key_and_value.first;
    const auto& match_value = match_key_and_value.second;
    chromeos::VariantDictionary bad_matcher_dict(exact_matcher);
    KeyValueStore bad_properties;
    bad_matcher_dict[match_key] = match_value;
    KeyValueStore::ConvertFromVariantDictionary(
        bad_matcher_dict, &bad_properties);
    EXPECT_EQ(set<string>(), store_.GetGroupsWithProperties(bad_properties))
        << "Failing match key: " << match_key;
  }
}

TEST_F(JsonStoreTest, ContainsGroupFindsExistingGroup) {
  store_.SetBool("group_a", "knob_1", bool());
  EXPECT_TRUE(store_.ContainsGroup("group_a"));
}

TEST_F(JsonStoreTest, ContainsGroupDoesNotFabricateGroups) {
  EXPECT_FALSE(store_.ContainsGroup("group_a"));
}

TEST_F(JsonStoreTest, DeleteGroupDeletesExistingGroup) {
  SetVerboseLevel(10);
  store_.SetBool("group_a", "knob_1", bool());
  store_.SetBool("group_a", "knob_2", bool());
  EXPECT_TRUE(store_.DeleteGroup("group_a"));
  EXPECT_CALL(log_, Log(_, _, HasSubstr("Could not find group"))).Times(2);
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_2", nullptr));
}

TEST_F(JsonStoreTest, DeleteGroupDeletesOnlySpecifiedGroup) {
  store_.SetBool("group_a", "knob_1", bool());
  store_.SetBool("group_b", "knob_1", bool());
  EXPECT_TRUE(store_.DeleteGroup("group_a"));
  EXPECT_FALSE(store_.GetBool("group_a", "knob_1", nullptr));
  EXPECT_TRUE(store_.GetBool("group_b", "knob_1", nullptr));
}

TEST_F(JsonStoreTest, DeleteGroupSucceedsOnMissingGroup) {
  store_.SetBool("group_a", "knob_1", bool());
  EXPECT_TRUE(store_.DeleteGroup("group_b"));
  EXPECT_TRUE(store_.GetBool("group_a", "knob_1", nullptr));
}

}  // namespace shill

// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <authpolicy/anonymizer.h>

namespace {

constexpr char kLog[] =
    "Starting fake search for user name USER_NAME\n"
    "Found 1 entry:\n"
    "userNameKey: USER_NAME\n";

constexpr char kMultiLog[] =
    "Starting fake search for key KEY_WITH_MULTIPLE_MATCHES\n"
    "Found 2 entries:\n"
    "userNameKey: USER_NAME\n"
    "userNameKey: DIFFERENT_NAME\n";

constexpr char kDifferentLogWithSameUserName[] =
    "Different string containing USER_NAME\n";

constexpr char kLogWithDifferentUserName[] = "userNameKey: DIFFERENT_NAME\n";

constexpr char kUserNameKey[] = "userNameKey";
constexpr char kUserName[] = "USER_NAME";
constexpr char kReplacement[] = "REPLACEMENT";
constexpr char kDifferentUserName[] = "DIFFERENT_NAME";

// Counts the number of occurrances of |substr| in |str|.
int CountOccurrances(const std::string& str, const std::string substr) {
  int count = 0;
  size_t pos = str.find(substr);
  while (pos != std::string::npos) {
    count++;
    pos = str.find(substr, pos + substr.size());
  }
  return count;
}

}  // namespace

namespace authpolicy {

class AnonymizerTest : public ::testing::Test {
 public:
  AnonymizerTest() {}
  ~AnonymizerTest() override {}

 protected:
  Anonymizer anonymizer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AnonymizerTest);
};

// Anonymizer does not change string if no replacements are set.
TEST_F(AnonymizerTest, NoChangeIfEmpty) {
  std::string anonymized_log = anonymizer_.Process(kLog);
  EXPECT_EQ(kLog, anonymized_log);
}

// Anonymizer replaces strings.
TEST_F(AnonymizerTest, ReplaceStrings) {
  EXPECT_NE(nullptr, strstr(kLog, kUserName));
  EXPECT_EQ(nullptr, strstr(kLog, kReplacement));

  anonymizer_.SetReplacement(kUserName, kReplacement);
  std::string anonymized_log = anonymizer_.Process(kLog);

  EXPECT_EQ(std::string::npos, anonymized_log.find(kUserName));
  EXPECT_NE(std::string::npos, anonymized_log.find(kReplacement));
}

// Anonymizer finds and replaces strings from search results.
TEST_F(AnonymizerTest, FindAndReplaceSearchValues) {
  anonymizer_.ReplaceSearchArg(kUserNameKey, kReplacement);
  std::string anonymized_log = anonymizer_.Process(kLog);

  EXPECT_EQ(std::string::npos, anonymized_log.find(kUserName));
  EXPECT_NE(std::string::npos, anonymized_log.find(kReplacement));

  // Even after resetting search arg replacements, the replacement kUserName ->
  // kReplacement should still hold.
  anonymizer_.ResetSearchArgReplacements();
  anonymized_log = anonymizer_.Process(kDifferentLogWithSameUserName);
  EXPECT_EQ(std::string::npos, anonymized_log.find(kUserName));
  EXPECT_NE(std::string::npos, anonymized_log.find(kReplacement));

  // However, the anonymizer should not pick up a different search result
  // anymore.
  anonymized_log = anonymizer_.Process(kLogWithDifferentUserName);
  EXPECT_NE(std::string::npos, anonymized_log.find(kDifferentUserName));
  EXPECT_EQ(std::string::npos, anonymized_log.find(kReplacement));
}

// Anonymizer finds multiple search results.
TEST_F(AnonymizerTest, FindMultipleSearchValues) {
  EXPECT_NE(nullptr, strstr(kMultiLog, kUserName));
  EXPECT_NE(nullptr, strstr(kMultiLog, kDifferentUserName));
  EXPECT_EQ(nullptr, strstr(kMultiLog, kReplacement));

  anonymizer_.ReplaceSearchArg(kUserNameKey, kReplacement);
  std::string anonymized_log = anonymizer_.Process(kMultiLog);

  EXPECT_EQ(std::string::npos, anonymized_log.find(kUserName));
  EXPECT_EQ(std::string::npos, anonymized_log.find(kDifferentUserName));
  EXPECT_EQ(2, CountOccurrances(anonymized_log, kReplacement));
}

// Anonymizer replaces KEY_123 before KEY_12 and KEY_12 before KEY_1.
TEST_F(AnonymizerTest, DoesNotReplaceShorterStringsFirst) {
  anonymizer_.SetReplacement("KEY_12", "second");
  anonymizer_.SetReplacement("KEY_123", "first");
  anonymizer_.SetReplacement("KEY_1", "third");
  constexpr char str[] = "KEY_1 KEY_123 KEY_12";
  std::string anonymized_str = anonymizer_.Process(str);
  EXPECT_EQ("third first second", anonymized_str);
}

}  // namespace authpolicy

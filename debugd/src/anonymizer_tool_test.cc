// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "anonymizer_tool.h"

using std::string;

namespace debugd {

class AnonymizerToolTest : public testing::Test {
 protected:
  string AnonymizeMACAddresses(const string& input) {
    return anonymizer_.AnonymizeMACAddresses(input);
  }

  string AnonymizeCustomPatterns(const string& input) {
    return anonymizer_.AnonymizeCustomPatterns(input);
  }

  string AnonymizeCustomPattern(
      const string& input, const string& tag, const string& pattern) {
    return anonymizer_.AnonymizeCustomPattern(input, tag, pattern);
  }

  AnonymizerTool anonymizer_;
};


TEST_F(AnonymizerToolTest, Anonymize) {
  EXPECT_EQ("", anonymizer_.Anonymize(""));
  EXPECT_EQ("foo\nbar\n", anonymizer_.Anonymize("foo\nbar\n"));

  // Make sure MAC address anonymization is invoked.
  EXPECT_EQ("02:46:8a:00:00:01", anonymizer_.Anonymize("02:46:8a:ce:13:57"));

  // Make sure custom pattern anonymization is invoked.
  EXPECT_EQ("cell-id-1", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
}

TEST_F(AnonymizerToolTest, AnonymizeMACAddresses) {
  EXPECT_EQ("", AnonymizeMACAddresses(""));
  EXPECT_EQ("foo\nbar\n", AnonymizeMACAddresses("foo\nbar\n"));
  EXPECT_EQ("11:22:33:44:55", AnonymizeMACAddresses("11:22:33:44:55"));
  EXPECT_EQ("aa:bb:cc:00:00:01", AnonymizeMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ("BSSID: aa:bb:cc:00:00:01 in the middle\n"
            "bb:cc:dd:00:00:02 start of line\n"
            "end of line aa:bb:cc:00:00:01\n"
            "no match across lines aa:bb:cc:\n"
            "dd:ee:ff two on the same line:\n"
            "x bb:cc:dd:00:00:02 cc:dd:ee:00:00:03 x\n",
            AnonymizeMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                                  "bb:cc:dd:ee:ff:00 start of line\n"
                                  "end of line aa:bb:cc:dd:ee:ff\n"
                                  "no match across lines aa:bb:cc:\n"
                                  "dd:ee:ff two on the same line:\n"
                                  "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember bb:cc:dd:00:00:02?",
            AnonymizeMACAddresses("Remember bB:Cc:DD:ee:ff:00?"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatterns) {
  EXPECT_EQ("", AnonymizeCustomPatterns(""));

  EXPECT_EQ("cell-id-1", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("cell-id-2", AnonymizeCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo cell-id-1 bar",
            AnonymizeCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo loc-area-code-1 bar",
            AnonymizeCustomPatterns("foo Location area code: 'A1B2' bar"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPattern) {
  const char kPattern[] = "\\b\\d+\\b";
  EXPECT_EQ("", AnonymizeCustomPattern("", "tag", kPattern));
  EXPECT_EQ("foo\nbar\n",
            AnonymizeCustomPattern("foo\nbar\n", "tag", kPattern));
  EXPECT_EQ("tag-1", AnonymizeCustomPattern("2345", "tag", kPattern));
  EXPECT_EQ("tag-2", AnonymizeCustomPattern("1234", "tag", kPattern));
  EXPECT_EQ("tag-1", AnonymizeCustomPattern("2345", "tag", kPattern));
  EXPECT_EQ("x1 tag-1 1x tag-2\ntag-1\n",
            AnonymizeCustomPattern("x1 2345 1x 1234\n2345\n", "tag", kPattern));
  EXPECT_EQ("foo-1", AnonymizeCustomPattern("1234", "foo", kPattern));
}

}  // namespace debugd

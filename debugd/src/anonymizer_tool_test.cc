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

  AnonymizerTool anonymizer_;
};


TEST_F(AnonymizerToolTest, Anonymize) {
  EXPECT_EQ("", anonymizer_.Anonymize(""));
  EXPECT_EQ("foo\nbar\n", anonymizer_.Anonymize("foo\nbar\n"));
  EXPECT_EQ("02:46:8a:00:00:01", anonymizer_.Anonymize("02:46:8a:ce:13:57"));
  EXPECT_EQ("02:46:8a:00:00:01", anonymizer_.Anonymize("02:46:8A:CE:13:57"));
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

}  // namespace debugd

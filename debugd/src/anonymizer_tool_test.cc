// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "debugd/src/anonymizer_tool.h"

using std::map;
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

  string AnonymizeAndroidAppStoragePaths(const string& input) {
    return anonymizer_.AnonymizeAndroidAppStoragePaths(input);
  }

  static string AnonymizeCustomPattern(
      const string& input, const string& pattern, map<string, string>* space) {
    return AnonymizerTool::AnonymizeCustomPattern(input, pattern, space);
  }

  AnonymizerTool anonymizer_;
};


TEST_F(AnonymizerToolTest, Anonymize) {
  EXPECT_EQ("", anonymizer_.Anonymize(""));
  EXPECT_EQ("foo\nbar\n", anonymizer_.Anonymize("foo\nbar\n"));

  // Make sure MAC address anonymization is invoked.
  EXPECT_EQ("[MAC OUI=02:46:8a IFACE=1]",
            anonymizer_.Anonymize("02:46:8a:ce:13:57"));

  // Make sure custom pattern anonymization is invoked.
  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
}

TEST_F(AnonymizerToolTest, AnonymizeMACAddresses) {
  EXPECT_EQ("", AnonymizeMACAddresses(""));
  EXPECT_EQ("foo\nbar\n", AnonymizeMACAddresses("foo\nbar\n"));
  EXPECT_EQ("11:22:33:44:55", AnonymizeMACAddresses("11:22:33:44:55"));
  EXPECT_EQ("[MAC OUI=aa:bb:cc IFACE=1]",
            AnonymizeMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ("00:00:00:00:00:00", AnonymizeMACAddresses("00:00:00:00:00:00"));
  EXPECT_EQ("ff:ff:ff:ff:ff:ff", AnonymizeMACAddresses("ff:ff:ff:ff:ff:ff"));
  EXPECT_EQ("BSSID: [MAC OUI=aa:bb:cc IFACE=1] in the middle\n"
            "[MAC OUI=bb:cc:dd IFACE=2] start of line\n"
            "end of line [MAC OUI=aa:bb:cc IFACE=1]\n"
            "no match across lines aa:bb:cc:\n"
            "dd:ee:ff two on the same line:\n"
            "x [MAC OUI=bb:cc:dd IFACE=2] [MAC OUI=cc:dd:ee IFACE=3] x\n",
            AnonymizeMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                                  "bb:cc:dd:ee:ff:00 start of line\n"
                                  "end of line aa:bb:cc:dd:ee:ff\n"
                                  "no match across lines aa:bb:cc:\n"
                                  "dd:ee:ff two on the same line:\n"
                                  "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember [MAC OUI=bb:cc:dd IFACE=2]?",
            AnonymizeMACAddresses("Remember bB:Cc:DD:ee:ff:00?"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPatterns) {
  EXPECT_EQ("", AnonymizeCustomPatterns(""));

  EXPECT_EQ("Cell ID: '1'", AnonymizeCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("Cell ID: '2'", AnonymizeCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo Cell ID: '1' bar",
            AnonymizeCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo Location area code: '1' bar",
            AnonymizeCustomPatterns("foo Location area code: 'A1B2' bar"));

  EXPECT_EQ("foo\na SSID='1' b\n'",
            AnonymizeCustomPatterns("foo\na SSID='Joe's' b\n'"));
  EXPECT_EQ("ssid '2'", AnonymizeCustomPatterns("ssid 'My AP'"));
  EXPECT_EQ("bssid 'aa:bb'", AnonymizeCustomPatterns("bssid 'aa:bb'"));

  EXPECT_EQ("Scan SSID - hexdump(len=6): 1\nfoo",
            AnonymizeCustomPatterns(
                "Scan SSID - hexdump(len=6): 47 6f 6f 67 6c 65\nfoo"));

  EXPECT_EQ("a\nb [SSID=1] [SSID=2] [SSID=foo\nbar] b",
            AnonymizeCustomPatterns(
                "a\nb [SSID=foo] [SSID=bar] [SSID=foo\nbar] b"));
}

TEST_F(AnonymizerToolTest, AnonymizeCustomPattern) {
  static const char kPattern[] = "(\\b(?i)id:? ')(\\d+)(')";
  map<string, string> space;
  EXPECT_EQ("", AnonymizeCustomPattern("", kPattern, &space));
  EXPECT_EQ("foo\nbar\n",
            AnonymizeCustomPattern("foo\nbar\n", kPattern, &space));
  EXPECT_EQ("id '1'", AnonymizeCustomPattern("id '2345'", kPattern, &space));
  EXPECT_EQ("id '2'", AnonymizeCustomPattern("id '1234'", kPattern, &space));
  EXPECT_EQ("id: '2'", AnonymizeCustomPattern("id: '1234'", kPattern, &space));
  EXPECT_EQ("ID: '1'", AnonymizeCustomPattern("ID: '2345'", kPattern, &space));
  EXPECT_EQ("x1 id '1' 1x id '2'\nid '1'\n",
            AnonymizeCustomPattern("x1 id '2345' 1x id '1234'\nid '2345'\n",
                                   kPattern, &space));
  space.clear();
  EXPECT_EQ("id '1'", AnonymizeCustomPattern("id '1234'", kPattern, &space));

  space.clear();
  EXPECT_EQ("x1z", AnonymizeCustomPattern("xyz", "()(y+)()", &space));
}

TEST_F(AnonymizerToolTest, AnonymizeAndroidAppStoragePaths) {
  EXPECT_EQ("", AnonymizeAndroidAppStoragePaths(""));
  EXPECT_EQ("foo\nbar\n", AnonymizeAndroidAppStoragePaths("foo\nbar\n"));

  constexpr char kDuOutput[] =
      "112K\t/home/root/deadbeef1234/android-data/data/system_de\n"
      // /data/data will be modified by the anonymizer.
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de/"
      "\xe3\x81\x82\n"
      "8.1K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de/"
      "\xe3\x81\x82\xe3\x81\x83\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/ef\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2\n"
      // /data/app won't.
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/app/pack.age1\n"
      // /data/user_de will.
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1\n"
      "78M\t/home/root/deadbeef1234/android-data/data/data\n";
  constexpr char kDuOutputRedacted[] =
      "112K\t/home/root/deadbeef1234/android-data/data/system_de\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/b_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_\n"
      // The non-ASCII directory names will become '*_'.
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_/*_\n"
      "8.1K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_/*_\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/e_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/app/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/b_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1\n"
      "78M\t/home/root/deadbeef1234/android-data/data/data\n";
  EXPECT_EQ(kDuOutputRedacted, AnonymizeAndroidAppStoragePaths(kDuOutput));
}

}  // namespace debugd

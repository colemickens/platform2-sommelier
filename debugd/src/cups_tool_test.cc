// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/cups_tool.h"

#include <gtest/gtest.h>

namespace debugd {

class CupsToolTest : public testing::Test {
 protected:
  CupsTool cups_tool_;
  const std::vector<std::string> known_schemes = {
    "usb://",
    "ipp://",
    "ipps://",
    "http://",
    "https://",
    "socket://",
    "lpd://",
    "ippusb://"
  };
};

// We reject empty and over-short URIs.
TEST_F(CupsToolTest, CatchShortUri) {
  EXPECT_FALSE(cups_tool_.UriSeemsReasonable(""));

  for (const std::string& sch : known_schemes) {
    EXPECT_FALSE(cups_tool_.UriSeemsReasonable(sch));
  }
}

// We reject garbage URIs.
TEST_F(CupsToolTest, CatchGarbageUri) {
  EXPECT_FALSE(cups_tool_.UriSeemsReasonable("aoeu"));
  EXPECT_FALSE(cups_tool_.UriSeemsReasonable("scheeeeeeme://bad"));
}

// We reject URIs with ``special'' characters.
TEST_F(CupsToolTest, CatchSpecialUri) {
  std::string special_uri("usb://looks.mostly.reasonable");
  EXPECT_TRUE(cups_tool_.UriSeemsReasonable(special_uri));

  special_uri.push_back(0x80);
  EXPECT_FALSE(cups_tool_.UriSeemsReasonable(special_uri));
}

// We pass URIs not violating the above conditions.
TEST_F(CupsToolTest, OkayUri) {
  for (std::string uri : known_schemes) {
    uri.append("looks.good.to.me:1313");
    EXPECT_TRUE(cups_tool_.UriSeemsReasonable(uri));
  }
}

TEST_F(CupsToolTest, PercentedUris) {
  std::string uri_with_space("lpd://127.0.0.1/PRINTER%20NAME");
  EXPECT_TRUE(cups_tool_.UriSeemsReasonable(uri_with_space));

  std::string incomplete("lpd://127.0.0.1/PRINTER%2");
  EXPECT_FALSE(cups_tool_.UriSeemsReasonable(incomplete));

  std::string questionable_uri("lpd://127.0.0.1/PRINTER%3F");
  EXPECT_FALSE(cups_tool_.UriSeemsReasonable(questionable_uri));
}

}  // namespace debugd

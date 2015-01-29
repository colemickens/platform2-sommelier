// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/privetd_conf_parser.h"

#include <map>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <chromeos/strings/string_utils.h>
#include <gtest/gtest.h>

using chromeos::string_utils::Join;
using chromeos::KeyValueStore;
using std::map;
using std::string;

namespace privetd {

namespace {

const char kWiFiBootstrapMode[] = "wifi_bootstrapping_mode";
const char kGcdBootstrapMode[] = "gcd_bootstrapping_mode";
const char kWiFiBootstrapInterfaces[] = "automatic_mode_interfaces";
const char kConnectTimeout[] = "connect_timeout_seconds";
const char kBootstrapTimeout[] = "bootstrap_timeout_seconds";
const char kMonitorTimeout[] = "monitor_timeout_seconds";

}  // namespace

class PrivetdConfParserTest : public testing::Test {
 public:
  using ConfDict = map<string, string>;

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_file_ = temp_dir_.path().Append("temp.conf");
  }

  void AssertInvalidConf(const ConfDict& conf_dict) {
    KeyValueStore store;
    FillKeyValueStore(conf_dict, &store);
    PrivetdConfigParser config;
    EXPECT_FALSE(config.Parse(store));
  }

  void FillKeyValueStore(const ConfDict& conf_dict, KeyValueStore* store) {
    std::vector<string> file_pieces;
    for (const auto& it : conf_dict) {
      file_pieces.push_back(Join('=', {it.first, it.second}));
    }
    string blob{Join('\n', file_pieces)};
    int expected_len = blob.length();
    CHECK(expected_len == base::WriteFile(temp_file_,
                                          blob.c_str(),
                                          expected_len));
    CHECK(store->Load(temp_file_));
  }

 private:
  base::FilePath temp_file_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(PrivetdConfParserTest, ShouldRejectInvalidTimeouts) {
  AssertInvalidConf({{kConnectTimeout, "-1"}});
  AssertInvalidConf({{kConnectTimeout, "a"}});
  AssertInvalidConf({{kConnectTimeout, ""}});
  AssertInvalidConf({{kConnectTimeout, "30 430"}});
}

TEST_F(PrivetdConfParserTest, ShouldRejectInvalidWiFiBootstrapModes) {
  AssertInvalidConf({{kWiFiBootstrapMode, ""}});
  AssertInvalidConf({{kWiFiBootstrapMode, "clown_shoes"}});
  AssertInvalidConf({{kWiFiBootstrapMode, "off is invalid"}});
  AssertInvalidConf({{kWiFiBootstrapMode, "30"}});
}

TEST_F(PrivetdConfParserTest, ShouldRejectInvalidGcdBootstrapModes) {
  AssertInvalidConf({{kGcdBootstrapMode, ""}});
  AssertInvalidConf({{kGcdBootstrapMode, "clown_shoes"}});
  AssertInvalidConf({{kGcdBootstrapMode, "off is invalid"}});
  AssertInvalidConf({{kGcdBootstrapMode, "30"}});
}

TEST_F(PrivetdConfParserTest, ShouldParseSettings) {
  const std::vector<std::string> kExpectedWiFiInterfaces{"eth1", "clown shoes"};
  const uint32_t kExpectedConnectTimeout{1};
  const uint32_t kExpectedBootstrapTimeout{2};
  const uint32_t kExpectedMonitorTimeout{3};
  const ConfDict conf_dict{
    {kWiFiBootstrapMode, "automatic"},
    {kGcdBootstrapMode, "automatic"},
    {kWiFiBootstrapInterfaces, Join(',', kExpectedWiFiInterfaces)},
    {kConnectTimeout, std::to_string(kExpectedConnectTimeout)},
    {kBootstrapTimeout, std::to_string(kExpectedBootstrapTimeout)},
    {kMonitorTimeout, std::to_string(kExpectedMonitorTimeout)},
  };
  KeyValueStore store;
  FillKeyValueStore(conf_dict, &store);
  PrivetdConfigParser parser;
  EXPECT_TRUE(parser.Parse(store));
  EXPECT_EQ(WiFiBootstrapMode::kAutomatic, parser.wifi_bootstrap_mode());
  EXPECT_EQ(GcdBootstrapMode::kAutomatic, parser.gcd_bootstrap_mode());
  EXPECT_EQ(kExpectedWiFiInterfaces, parser.automatic_wifi_interfaces());
  EXPECT_EQ(kExpectedConnectTimeout, parser.connect_timeout_seconds());
  EXPECT_EQ(kExpectedBootstrapTimeout, parser.bootstrap_timeout_seconds());
  EXPECT_EQ(kExpectedMonitorTimeout, parser.monitor_timeout_seconds());
}

}  // namespace privetd

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/privet/privetd_conf_parser.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <chromeos/strings/string_utils.h>
#include <gtest/gtest.h>

#include "buffet/privet/security_delegate.h"

using chromeos::string_utils::Join;
using chromeos::KeyValueStore;
using std::map;
using std::string;

namespace privetd {

namespace {

const char kWiFiBootstrapMode[] = "wifi_bootstrapping_mode";
const char kPairingModes[] = "pairing_modes";
const char kEmbeddedCodePath[] = "embedded_code_path";

}  // namespace

class PrivetdConfParserTest : public testing::Test {
 public:
  using ConfDict = map<string, string>;

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    temp_file_ = temp_dir_.path().Append("temp.conf");
  }

  void FillKeyValueStore(const ConfDict& conf_dict, KeyValueStore* store) {
    std::vector<string> file_pieces;
    for (const auto& it : conf_dict) {
      file_pieces.push_back(Join("=", it.first, it.second));
    }
    string blob{Join("\n", file_pieces)};
    int expected_len = blob.length();
    CHECK(expected_len ==
          base::WriteFile(temp_file_, blob.c_str(), expected_len));
    CHECK(store->Load(temp_file_));
  }

 private:
  base::FilePath temp_file_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(PrivetdConfParserTest, ShouldParseSettings) {
  const std::set<PairingType> kExpectedPairingModes{PairingType::kEmbeddedCode,
                                                    PairingType::kPinCode};
  static const char kExpectedEmbeddedCodePath[]{"123ABC"};
  const ConfDict conf_dict{
      {kWiFiBootstrapMode, "automatic"},
      {kPairingModes, "pinCode"},
      {kEmbeddedCodePath, kExpectedEmbeddedCodePath},
  };
  KeyValueStore store;
  FillKeyValueStore(conf_dict, &store);
  PrivetdConfigParser parser;
  EXPECT_TRUE(parser.Parse(store));
  EXPECT_TRUE(parser.wifi_auto_setup_enabled());
  EXPECT_EQ(kExpectedPairingModes, parser.pairing_modes());
  EXPECT_EQ(kExpectedEmbeddedCodePath, parser.embedded_code_path().value());
}

TEST_F(PrivetdConfParserTest, CriticalDefaults) {
  PrivetdConfigParser parser;
  EXPECT_TRUE(parser.wifi_auto_setup_enabled());
  EXPECT_EQ(std::set<PairingType>{PairingType::kPinCode},
            parser.pairing_modes());
  EXPECT_TRUE(parser.embedded_code_path().empty());
}

}  // namespace privetd

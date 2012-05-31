// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/scope_logger.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace shill {

class ScopeLoggerTest : public testing::Test {
 protected:
  ScopeLoggerTest() {}

  void TearDown() {
    logger_.set_verbose_level(0);
    logger_.DisableAllScopes();
  }

  ScopeLogger logger_;
};

TEST_F(ScopeLoggerTest, DefaultConstruction) {
  for (int scope = 0; scope < ScopeLogger::kNumScopes; ++scope) {
    for (int verbose_level = 0; verbose_level < 5; ++verbose_level) {
      EXPECT_FALSE(logger_.IsLogEnabled(
          static_cast<ScopeLogger::Scope>(scope), verbose_level));
    }
  }
}

TEST_F(ScopeLoggerTest, GetAllScopeNames) {
  EXPECT_EQ("cellular+"
            "connection+"
            "crypto+"
            "daemon+"
            "dbus+"
            "device+"
            "dhcp+"
            "dns+"
            "ethernet+"
            "http+"
            "httpproxy+"
            "inet+"
            "manager+"
            "metrics+"
            "modem+"
            "portal+"
            "power+"
            "profile+"
            "property+"
            "resolver+"
            "route+"
            "rtnl+"
            "service+"
            "storage+"
            "task+"
            "vpn+"
            "wifi+"
            "wimax",
      logger_.GetAllScopeNames());
}

TEST_F(ScopeLoggerTest, GetEnabledScopeNames) {
  EXPECT_EQ("", logger_.GetEnabledScopeNames());

  logger_.SetScopeEnabled(ScopeLogger::kWiFi, true);
  EXPECT_EQ("wifi", logger_.GetEnabledScopeNames());

  logger_.SetScopeEnabled(ScopeLogger::kService, true);
  EXPECT_EQ("service+wifi", logger_.GetEnabledScopeNames());

  logger_.SetScopeEnabled(ScopeLogger::kVPN, true);
  EXPECT_EQ("service+vpn+wifi", logger_.GetEnabledScopeNames());

  logger_.SetScopeEnabled(ScopeLogger::kWiFi, false);
  EXPECT_EQ("service+vpn", logger_.GetEnabledScopeNames());
}

TEST_F(ScopeLoggerTest, EnableScopesByName) {
  logger_.EnableScopesByName("");
  EXPECT_EQ("", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("+wifi");
  EXPECT_EQ("wifi", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("+service");
  EXPECT_EQ("service+wifi", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("+vpn+wifi");
  EXPECT_EQ("service+vpn+wifi", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("-wifi");
  EXPECT_EQ("service+vpn", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("-vpn-service+wifi");
  EXPECT_EQ("wifi", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("+-wifi-");
  EXPECT_EQ("", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("-vpn+vpn+wifi-wifi");
  EXPECT_EQ("vpn", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("wifi");
  EXPECT_EQ("wifi", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("");
  EXPECT_EQ("", logger_.GetEnabledScopeNames());
}

TEST_F(ScopeLoggerTest, EnableScopesByNameWithUnknownScopeName) {
  logger_.EnableScopesByName("foo");
  EXPECT_EQ("", logger_.GetEnabledScopeNames());

  logger_.EnableScopesByName("wifi+foo+vpn");
  EXPECT_EQ("vpn+wifi", logger_.GetEnabledScopeNames());
}

TEST_F(ScopeLoggerTest, SetScopeEnabled) {
  EXPECT_FALSE(logger_.IsLogEnabled(ScopeLogger::kService, 0));

  logger_.SetScopeEnabled(ScopeLogger::kService, true);
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 0));

  logger_.SetScopeEnabled(ScopeLogger::kService, false);
  EXPECT_FALSE(logger_.IsLogEnabled(ScopeLogger::kService, 0));
}

TEST_F(ScopeLoggerTest, SetVerboseLevel) {
  logger_.SetScopeEnabled(ScopeLogger::kService, true);
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 0));
  EXPECT_FALSE(logger_.IsLogEnabled(ScopeLogger::kService, 1));
  EXPECT_FALSE(logger_.IsLogEnabled(ScopeLogger::kService, 2));

  logger_.set_verbose_level(1);
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 0));
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 1));
  EXPECT_FALSE(logger_.IsLogEnabled(ScopeLogger::kService, 2));

  logger_.set_verbose_level(2);
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 0));
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 1));
  EXPECT_TRUE(logger_.IsLogEnabled(ScopeLogger::kService, 2));
}

}  // namespace shill

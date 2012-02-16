// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/resolver.h"

#include <base/file_util.h>
#include <base/scoped_temp_dir.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>
#include <gtest/gtest.h>

#include "shill/ipconfig.h"
#include "shill/mock_control.h"

using std::string;
using std::vector;
using testing::Test;

namespace shill {

namespace {
const char kTestDeviceName0[] = "netdev0";
const char kNameServer0[] = "8.8.8.8";
const char kNameServer1[] = "8.8.9.9";
const char kSearchDomain0[] = "chromium.org";
const char kSearchDomain1[] = "google.com";
const char kExpectedOutput[] =
  "nameserver 8.8.8.8\n"
  "nameserver 8.8.9.9\n"
  "search chromium.org google.com\n"
  "options single-request\n";
}  // namespace {}

class ResolverTest : public Test {
 public:
  ResolverTest() : resolver_(Resolver::GetInstance()) {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.path().Append("resolver");
    resolver_->set_path(path_);
  }

  virtual void TearDown() {
    resolver_->set_path(FilePath(""));  // Don't try to save the store.
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  string ReadFile();

  ScopedTempDir temp_dir_;
  Resolver *resolver_;
  FilePath path_;
};

string ResolverTest::ReadFile() {
  string data;
  EXPECT_TRUE(file_util::ReadFileToString(resolver_->path_, &data));
  return data;
}

TEST_F(ResolverTest, NonEmpty) {
  EXPECT_FALSE(file_util::PathExists(path_));
  EXPECT_TRUE(resolver_->ClearDNS());

  // Add DNS info from an IPConfig entry
  MockControl control;
  IPConfigRefPtr ipconfig(new IPConfig(&control, kTestDeviceName0));
  IPConfig::Properties properties;
  properties.dns_servers.push_back(kNameServer0);
  properties.dns_servers.push_back(kNameServer1);
  properties.domain_search.push_back(kSearchDomain0);
  properties.domain_search.push_back(kSearchDomain1);
  ipconfig->UpdateProperties(properties, true);

  EXPECT_TRUE(resolver_->SetDNSFromIPConfig(ipconfig));
  EXPECT_TRUE(file_util::PathExists(path_));
  EXPECT_EQ(kExpectedOutput, ReadFile());

  EXPECT_TRUE(resolver_->ClearDNS());
}

TEST_F(ResolverTest, Empty) {
  EXPECT_FALSE(file_util::PathExists(path_));

  // Use empty ifconfig
  MockControl control;
  IPConfigRefPtr ipconfig(new IPConfig(&control, kTestDeviceName0));
  IPConfig::Properties properties;
  ipconfig->UpdateProperties(properties, true);

  EXPECT_TRUE(resolver_->SetDNSFromIPConfig(ipconfig));
  EXPECT_FALSE(file_util::PathExists(path_));
}

}  // namespace shill

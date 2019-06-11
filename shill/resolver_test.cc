// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/resolver.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

using base::FilePath;
using std::string;
using std::vector;
using testing::Test;

namespace shill {

namespace {
const char kNameServer0[] = "8.8.8.8";
const char kNameServer1[] = "8.8.9.9";
const char kNameServer2[] = "2001:4860:4860:0:0:0:0:8888";
const char kNameServerEvil[] = "8.8.8.8\noptions debug";
const char kNameServerSubtlyEvil[] = "3.14.159.265";
const char kSearchDomain0[] = "chromium.org";
const char kSearchDomain1[] = "google.com";
const char kSearchDomain2[] = "crbug.com";
const char kSearchDomainEvil[] = "google.com\nnameserver 6.6.6.6";
const char kSearchDomainSubtlyEvil[] = "crate&barrel.com";
const char kExpectedOutput[] =
    "nameserver 8.8.8.8\n"
    "nameserver 8.8.9.9\n"
    "nameserver 2001:4860:4860::8888\n"
    "search chromium.org google.com\n"
    "options single-request timeout:1 attempts:5\n";
const char kExpectedIgnoredSearchOutput[] =
    "nameserver 8.8.8.8\n"
    "nameserver 8.8.9.9\n"
    "nameserver 2001:4860:4860::8888\n"
    "search google.com\n"
    "options single-request timeout:1 attempts:5\n";
}  // namespace

class ResolverTest : public Test {
 public:
  ResolverTest() : resolver_(Resolver::GetInstance()) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().Append("resolver");
    resolver_->set_path(path_);
  }

  void TearDown() override {
    resolver_->set_path(FilePath(""));  // Don't try to save the store.
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  string ReadFile();

  base::ScopedTempDir temp_dir_;
  Resolver* resolver_;
  FilePath path_;
};

string ResolverTest::ReadFile() {
  string data;
  EXPECT_TRUE(base::ReadFileToString(resolver_->path_, &data));
  return data;
}

TEST_F(ResolverTest, NonEmpty) {
  EXPECT_FALSE(base::PathExists(path_));
  EXPECT_TRUE(resolver_->ClearDNS());

  vector<string> dns_servers;
  vector<string> domain_search;
  dns_servers.push_back(kNameServer0);
  dns_servers.push_back(kNameServer1);
  dns_servers.push_back(kNameServer2);
  domain_search.push_back(kSearchDomain0);
  domain_search.push_back(kSearchDomain1);

  EXPECT_TRUE(resolver_->SetDNSFromLists(dns_servers, domain_search));
  EXPECT_TRUE(base::PathExists(path_));
  EXPECT_EQ(kExpectedOutput, ReadFile());

  EXPECT_TRUE(resolver_->ClearDNS());
}

TEST_F(ResolverTest, Sanitize) {
  EXPECT_FALSE(base::PathExists(path_));
  EXPECT_TRUE(resolver_->ClearDNS());

  vector<string> dns_servers;
  vector<string> domain_search;

  dns_servers.push_back(kNameServer0);
  dns_servers.push_back(kNameServerEvil);
  dns_servers.push_back(kNameServer1);
  dns_servers.push_back(kNameServerSubtlyEvil);
  dns_servers.push_back(kNameServer2);

  domain_search.push_back(kSearchDomainEvil);
  domain_search.push_back(kSearchDomain0);
  domain_search.push_back(kSearchDomain1);
  domain_search.push_back(kSearchDomainSubtlyEvil);

  EXPECT_TRUE(resolver_->SetDNSFromLists(dns_servers, domain_search));
  EXPECT_TRUE(base::PathExists(path_));
  EXPECT_EQ(kExpectedOutput, ReadFile());

  EXPECT_TRUE(resolver_->ClearDNS());
}

TEST_F(ResolverTest, Empty) {
  EXPECT_FALSE(base::PathExists(path_));

  vector<string> dns_servers;
  vector<string> domain_search;

  EXPECT_TRUE(resolver_->SetDNSFromLists(dns_servers, domain_search));
  EXPECT_FALSE(base::PathExists(path_));
}

TEST_F(ResolverTest, IgnoredSearchList) {
  EXPECT_FALSE(base::PathExists(path_));
  EXPECT_TRUE(resolver_->ClearDNS());

  vector<string> dns_servers;
  vector<string> domain_search;
  dns_servers.push_back(kNameServer0);
  dns_servers.push_back(kNameServer1);
  dns_servers.push_back(kNameServer2);
  domain_search.push_back(kSearchDomain0);
  domain_search.push_back(kSearchDomain1);
  vector<string> ignored_search;
  ignored_search.push_back(kSearchDomain0);
  ignored_search.push_back(kSearchDomain2);
  resolver_->set_ignored_search_list(ignored_search);
  EXPECT_TRUE(resolver_->SetDNSFromLists(dns_servers, domain_search));
  EXPECT_TRUE(base::PathExists(path_));
  EXPECT_EQ(kExpectedIgnoredSearchOutput, ReadFile());

  EXPECT_TRUE(resolver_->ClearDNS());
}

}  // namespace shill

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_operator_info.h"

#include <base/file_util.h>
#include <gtest/gtest.h>

#include "shill/glib.h"

using base::FilePath;
using std::map;
using std::string;
using std::vector;

namespace shill {

namespace {

const char kTestInfoFileContent[] =
    "#\n"
    "# Comments\n"
    "#\n"
    "serviceproviders:3.0\n"
    "country:us\n"
    "\n"
    "# TestProvider1\n"
    "provider:1,1,0,1\n"
    "identifier:provider1identifier\n"
    "name:,TestProvider1\n"
    "mccmnc:000001,0,000002,0\n"
    "sid:1,0,2,0,3,0\n"
    "olp:POST,https://testurl,imei=${imei}&iccid=${iccid}\n"
    "apn:2,testprovider1apn,,\n"
    "name:en,Test Provider 1\n"
    "name:de,Testmobilfunkanbieter 1\n"
    "\n"
    "# TestProvider2\n"
    "provider:1,2,1,0\n"
    "identifier:provider2identifier\n"
    "name:,TestProviderTwo\n"
    "name:,TestProvider2\n"
    "mccmnc:100001,1,100002,0\n"
    "sid:4,0,5,1\n"
    "olp:,https://testurl2,\n"
    "olp:,https://testurl3,\n"
    "apn:1,testprovider2apn,,\n"
    "name:,Test Provider 2\n"
    "apn:1,testprovider2apn2,testusername,testpassword\n"
    "name:tr,Test Operatoru 2\n";

}  // namespace

class CellularOperatorInfoTest : public testing::Test {
 public:
  CellularOperatorInfoTest() {}

 protected:
  void SetUp() {
    ASSERT_TRUE(base::CreateTemporaryFile(&info_file_path_));
    ASSERT_EQ(arraysize(kTestInfoFileContent),
              file_util::WriteFile(info_file_path_, kTestInfoFileContent,
                                   arraysize(kTestInfoFileContent)));
  }

  void TruncateFile() {
    FILE *file = base::OpenFile(info_file_path_, "rw");
    base::TruncateFile(file);
    base::CloseFile(file);
  }

  void WriteToFile(const char *content) {
    ASSERT_EQ(strlen(content),
              file_util::WriteFile(info_file_path_, content, strlen(content)));
  }

  void TruncateAndWriteToFile(const char *content) {
    TruncateFile();
    WriteToFile(content);
  }

  void TearDown() {
    ASSERT_TRUE(base::DeleteFile(info_file_path_, false));
  }

  GLib glib_;
  FilePath info_file_path_;
  CellularOperatorInfo info_;
};

TEST_F(CellularOperatorInfoTest, ParseSuccess) {
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(2, info_.operators().size());

  const CellularOperatorInfo::CellularOperator *provider = info_.operators()[0];
  EXPECT_FALSE(provider->is_primary());
  EXPECT_TRUE(provider->requires_roaming());
  EXPECT_EQ("us", provider->country());
  EXPECT_EQ("provider1identifier", provider->identifier());
  EXPECT_EQ(1, provider->name_list().size());
  EXPECT_TRUE(provider->name_list()[0].language.empty());
  EXPECT_EQ("TestProvider1", provider->name_list()[0].name);
  EXPECT_EQ(2, provider->mccmnc_list().size());
  EXPECT_EQ("000001", provider->mccmnc_list()[0]);
  EXPECT_EQ("000002", provider->mccmnc_list()[1]);
  EXPECT_EQ(3, provider->sid_list().size());
  EXPECT_EQ("1", provider->sid_list()[0]);
  EXPECT_EQ("2", provider->sid_list()[1]);
  EXPECT_EQ("3", provider->sid_list()[2]);
  EXPECT_EQ(1, provider->olp_list().size());
  EXPECT_EQ("https://testurl", provider->olp_list()[0]->GetURL());
  EXPECT_EQ("POST", provider->olp_list()[0]->GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}",
            provider->olp_list()[0]->GetPostData());
  EXPECT_EQ(1, provider->apn_list().size());
  EXPECT_EQ("testprovider1apn", provider->apn_list()[0]->apn);
  EXPECT_TRUE(provider->apn_list()[0]->username.empty());
  EXPECT_TRUE(provider->apn_list()[0]->password.empty());
  EXPECT_EQ(2, provider->apn_list()[0]->name_list.size());
  EXPECT_EQ("en", provider->apn_list()[0]->name_list[0].language);
  EXPECT_EQ("Test Provider 1", provider->apn_list()[0]->name_list[0].name);
  EXPECT_EQ("de", provider->apn_list()[0]->name_list[1].language);
  EXPECT_EQ("Testmobilfunkanbieter 1",
            provider->apn_list()[0]->name_list[1].name);

  const CellularOperatorInfo::CellularOperator *provider2 =
      info_.operators()[1];
  EXPECT_TRUE(provider2->is_primary());
  EXPECT_FALSE(provider2->requires_roaming());
  EXPECT_EQ("us", provider2->country());
  EXPECT_EQ("provider2identifier", provider2->identifier());
  EXPECT_EQ(2, provider2->name_list().size());
  EXPECT_TRUE(provider2->name_list()[0].language.empty());
  EXPECT_EQ("TestProviderTwo", provider2->name_list()[0].name);
  EXPECT_TRUE(provider2->name_list()[1].language.empty());
  EXPECT_EQ("TestProvider2", provider2->name_list()[1].name);
  EXPECT_EQ(2, provider2->mccmnc_list().size());
  EXPECT_EQ("100001", provider2->mccmnc_list()[0]);
  EXPECT_EQ("100002", provider2->mccmnc_list()[1]);
  EXPECT_EQ(2, provider2->sid_list().size());
  EXPECT_EQ("4", provider2->sid_list()[0]);
  EXPECT_EQ("5", provider2->sid_list()[1]);
  EXPECT_EQ(2, provider2->olp_list().size());
  EXPECT_EQ("https://testurl2", provider2->olp_list()[0]->GetURL());
  EXPECT_TRUE(provider2->olp_list()[0]->GetMethod().empty());
  EXPECT_TRUE(provider2->olp_list()[0]->GetPostData().empty());
  EXPECT_EQ("https://testurl3", provider2->olp_list()[1]->GetURL());
  EXPECT_TRUE(provider2->olp_list()[1]->GetMethod().empty());
  EXPECT_TRUE(provider2->olp_list()[1]->GetPostData().empty());
  EXPECT_EQ(2, provider2->apn_list().size());
  EXPECT_EQ("testprovider2apn", provider2->apn_list()[0]->apn);
  EXPECT_TRUE(provider2->apn_list()[0]->username.empty());
  EXPECT_TRUE(provider2->apn_list()[0]->password.empty());
  EXPECT_EQ(1, provider2->apn_list()[0]->name_list.size());
  EXPECT_TRUE(provider2->apn_list()[0]->name_list[0].language.empty());
  EXPECT_EQ("Test Provider 2",
            provider2->apn_list()[0]->name_list[0].name);
  EXPECT_EQ("testprovider2apn2", provider2->apn_list()[1]->apn);
  EXPECT_EQ("testusername", provider2->apn_list()[1]->username);
  EXPECT_EQ("testpassword", provider2->apn_list()[1]->password);
  EXPECT_EQ(1, provider2->apn_list()[1]->name_list.size());
  EXPECT_EQ("tr", provider2->apn_list()[1]->name_list[0].language);
  EXPECT_EQ("Test Operatoru 2",
            provider2->apn_list()[1]->name_list[0].name);
}

TEST_F(CellularOperatorInfoTest, GetCellularOperatorByMCCMNC) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("1"));
  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("000003"));
  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("bananas"));
  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("abcd"));

  const CellularOperatorInfo::CellularOperator *provider = NULL;
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("000001")));
  EXPECT_EQ(info_.operators()[0], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("100001")));
  EXPECT_EQ(info_.operators()[1], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("000002")));
  EXPECT_EQ(info_.operators()[0], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("100002")));
  EXPECT_EQ(info_.operators()[1], provider);
}

TEST_F(CellularOperatorInfoTest, GetCellularOperatorBySID) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  EXPECT_FALSE(info_.GetCellularOperatorBySID("000001"));
  EXPECT_FALSE(info_.GetCellularOperatorBySID("000002"));
  EXPECT_FALSE(info_.GetCellularOperatorBySID("100001"));
  EXPECT_FALSE(info_.GetCellularOperatorBySID("100002"));
  EXPECT_FALSE(info_.GetCellularOperatorBySID("banana"));

  const CellularOperatorInfo::CellularOperator *provider = NULL;
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("1")));
  EXPECT_EQ(info_.operators()[0], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("4")));
  EXPECT_EQ(info_.operators()[1], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("2")));
  EXPECT_EQ(info_.operators()[0], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("5")));
  EXPECT_EQ(info_.operators()[1], provider);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("3")));
  EXPECT_EQ(info_.operators()[0], provider);
}

TEST_F(CellularOperatorInfoTest, GetCellularOperators) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  const vector<const CellularOperatorInfo::CellularOperator *> *list = NULL;
  EXPECT_FALSE(info_.GetCellularOperators("banana"));
  EXPECT_FALSE(info_.GetCellularOperators("TestProvider2"));

  EXPECT_TRUE((list = info_.GetCellularOperators("TestProvider1")));
  EXPECT_EQ(1, list->size());
  EXPECT_EQ("testprovider1apn", (*list)[0]->apn_list()[0]->apn);

  EXPECT_FALSE(info_.GetCellularOperators("TestProvider2"));

  EXPECT_TRUE((list = info_.GetCellularOperators("TestProviderTwo")));
  EXPECT_EQ(1, list->size());
  EXPECT_EQ("testprovider2apn", (*list)[0]->apn_list()[0]->apn);
}

TEST_F(CellularOperatorInfoTest, GetOLPByMCCMNC) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  const CellularOperatorInfo::OLP *olp = NULL;
  EXPECT_TRUE((olp = info_.GetOLPByMCCMNC("000001")));
  EXPECT_EQ("https://testurl", olp->GetURL());
  EXPECT_EQ("POST", olp->GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPByMCCMNC("000002")));
  EXPECT_EQ("https://testurl", olp->GetURL());
  EXPECT_EQ("POST", olp->GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPByMCCMNC("100001")));
  EXPECT_EQ("https://testurl3", olp->GetURL());
  EXPECT_TRUE(olp->GetMethod().empty());
  EXPECT_TRUE(olp->GetPostData().empty());

  EXPECT_TRUE((olp = info_.GetOLPByMCCMNC("100002")));
  EXPECT_EQ("https://testurl2", olp->GetURL());
  EXPECT_TRUE(olp->GetMethod().empty());
  EXPECT_TRUE(olp->GetPostData().empty());

  EXPECT_FALSE(info_.GetOLPByMCCMNC("000003"));
  EXPECT_FALSE(info_.GetOLPByMCCMNC("000004"));
}

TEST_F(CellularOperatorInfoTest, GetOLPBySID) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  const CellularOperatorInfo::OLP *olp = NULL;
  EXPECT_TRUE((olp = info_.GetOLPBySID("1")));
  EXPECT_EQ("https://testurl", olp->GetURL());
  EXPECT_EQ("POST", olp->GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPBySID("2")));
  EXPECT_EQ("https://testurl", olp->GetURL());
  EXPECT_EQ("POST", olp->GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPBySID("3")));
  EXPECT_EQ("https://testurl", olp->GetURL());
  EXPECT_EQ("POST", olp->GetMethod());
  EXPECT_EQ("imei=${imei}&iccid=${iccid}", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPBySID("4")));
  EXPECT_EQ("https://testurl2", olp->GetURL());
  EXPECT_TRUE(olp->GetMethod().empty());
  EXPECT_TRUE(olp->GetPostData().empty());

  EXPECT_TRUE((olp = info_.GetOLPBySID("5")));
  EXPECT_EQ("https://testurl3", olp->GetURL());
  EXPECT_TRUE(olp->GetMethod().empty());
  EXPECT_TRUE(olp->GetPostData().empty());

  EXPECT_FALSE(info_.GetOLPBySID("6"));
  EXPECT_FALSE(info_.GetOLPBySID("7"));
}

TEST_F(CellularOperatorInfoTest, BadServiceProvidersLine) {
  TruncateFile();

  // Invalid first line.
  TruncateAndWriteToFile(
      "# Bla bla bla\n"
      "# Blabbidy boo\n"
      "serviceproviders:2.3\n");
  EXPECT_FALSE(info_.Load(info_file_path_));

  // Valid first line
  TruncateAndWriteToFile(
      "# Bla bla bla\n"
      "# Blabbidy boo\n"
      "serviceproviders:3.0\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
}

TEST_F(CellularOperatorInfoTest, HandleProvider) {
  // Invalid provider entry
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# Invalid provider entry\n"
      "provider:0,0,0\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# Invalid provider entry\n"
      "provider:1,1,0,1\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_FALSE(info_.operators()[0]->is_primary());
  EXPECT_TRUE(info_.operators()[0]->requires_roaming());
  EXPECT_TRUE(info_.operators()[0]->country().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# Invalid provider entry\n"
      "country:us\n"
      "provider:1,1,1,0\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_TRUE(info_.operators()[0]->is_primary());
  EXPECT_FALSE(info_.operators()[0]->requires_roaming());
  EXPECT_EQ("us", info_.operators()[0]->country());
}

TEST_F(CellularOperatorInfoTest, HandleMCCMNC) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# MCCMNC entry without a provider.\n"
      "mccmnc:1,1\n");
  EXPECT_FALSE(info_.Load(info_file_path_));

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# Empty MCCMNC entry.\n"
      "provider:1,1,0,1\n"
      "mccmnc:\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# MCCMNC entry has odd number of values.\n"
      "provider:1,1,0,1\n"
      "mccmnc:000001,0,000002\n");

  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# MCCMNC entry in this one is good.\n"
      "provider:1,1,0,1\n"
      "mccmnc:000001,0,000002,3\n");

  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(2, info_.operators()[0]->mccmnc_list().size());
  EXPECT_EQ("000001", info_.operators()[0]->mccmnc_list()[0]);
  EXPECT_EQ("000002", info_.operators()[0]->mccmnc_list()[1]);
  EXPECT_EQ(info_.operators()[0], info_.GetCellularOperatorByMCCMNC("000001"));
  EXPECT_EQ(info_.operators()[0], info_.GetCellularOperatorByMCCMNC("000002"));
  EXPECT_EQ(NULL, info_.GetCellularOperatorByMCCMNC("000003"));
}

TEST_F(CellularOperatorInfoTest, HandleName) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# No provider entry\n"
      "name:,\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "# Name has incorrect number of fields.\n"
      "name:,,\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "# Name is valid.\n"
      "name:en,Test Name\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(1, info_.operators()[0]->name_list().size());
  EXPECT_EQ("en", info_.operators()[0]->name_list()[0].language);
  EXPECT_EQ("Test Name", info_.operators()[0]->name_list()[0].name);

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "# More than one valid names.\n"
      "name:en,Test Name\n"
      "name:,Other Name\n"
      "name:de,\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(3, info_.operators()[0]->name_list().size());
  EXPECT_EQ("en", info_.operators()[0]->name_list()[0].language);
  EXPECT_EQ("Test Name", info_.operators()[0]->name_list()[0].name);
  EXPECT_TRUE(info_.operators()[0]->name_list()[1].language.empty());
  EXPECT_EQ("Other Name", info_.operators()[0]->name_list()[1].name);
  EXPECT_EQ("de", info_.operators()[0]->name_list()[2].language);
  EXPECT_TRUE(info_.operators()[0]->name_list()[2].name.empty());
}

TEST_F(CellularOperatorInfoTest, HandleAPN) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# No provider\n"
      "apn:,,,\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,0,0,0\n"
      "# Badly formed apn line.\n"
      "apn:,\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,0,0,0\n"
      "apn:0,testapn,testusername,testpassword\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(1, info_.operators()[0]->apn_list().size());
  EXPECT_EQ("testapn", info_.operators()[0]->apn_list()[0]->apn);
  EXPECT_EQ("testusername", info_.operators()[0]->apn_list()[0]->username);
  EXPECT_EQ("testpassword", info_.operators()[0]->apn_list()[0]->password);

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,0,0,0\n"
      "apn:0,apn1,user1,password1\n"
      "apn:2,apn2,user2,password2\n"
      "name:en,Apn Name\n"
      "name:de,Apn Name2\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(2, info_.operators()[0]->apn_list().size());
  EXPECT_EQ("apn1", info_.operators()[0]->apn_list()[0]->apn);
  EXPECT_EQ("user1", info_.operators()[0]->apn_list()[0]->username);
  EXPECT_EQ("password1", info_.operators()[0]->apn_list()[0]->password);
  EXPECT_EQ("apn2", info_.operators()[0]->apn_list()[1]->apn);
  EXPECT_EQ("user2", info_.operators()[0]->apn_list()[1]->username);
  EXPECT_EQ("password2", info_.operators()[0]->apn_list()[1]->password);

  EXPECT_TRUE(info_.operators()[0]->apn_list()[0]->name_list.empty());
  EXPECT_EQ(2, info_.operators()[0]->apn_list()[1]->name_list.size());
  EXPECT_EQ("en",
            info_.operators()[0]->apn_list()[1]->name_list[0].language);
  EXPECT_EQ("Apn Name",
            info_.operators()[0]->apn_list()[1]->name_list[0].name);
  EXPECT_EQ("de",
            info_.operators()[0]->apn_list()[1]->name_list[1].language);
  EXPECT_EQ("Apn Name2",
            info_.operators()[0]->apn_list()[1]->name_list[1].name);
}

TEST_F(CellularOperatorInfoTest, HandleSID) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# SID entry without a provider.\n"
      "sid:1,1\n");
  EXPECT_FALSE(info_.Load(info_file_path_));

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# Empty SID entry.\n"
      "provider:1,1,0,1\n"
      "sid:\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# SID entry has odd number of values.\n"
      "provider:1,1,0,1\n"
      "sid:1,0,2\n");

  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# SID entry in this one is good.\n"
      "provider:1,1,0,1\n"
      "sid:1,0,2,3\n");

  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(2, info_.operators()[0]->sid_list().size());
  EXPECT_EQ("1", info_.operators()[0]->sid_list()[0]);
  EXPECT_EQ("2", info_.operators()[0]->sid_list()[1]);
  EXPECT_EQ(info_.operators()[0], info_.GetCellularOperatorBySID("1"));
  EXPECT_EQ(info_.operators()[0], info_.GetCellularOperatorBySID("2"));
  EXPECT_EQ(NULL, info_.GetCellularOperatorBySID("3"));
}

TEST_F(CellularOperatorInfoTest, HandleIdentifier) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# No provider entry.\n"
      "identifier:test-id\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "identifier:test-id\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ("test-id", info_.operators()[0]->identifier());
}

TEST_F(CellularOperatorInfoTest, HandleActivationCode) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# No provider entry.\n"
      "activation-code:test-code\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "activation-code:test-code\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ("test-code", info_.operators()[0]->activation_code());
}

TEST_F(CellularOperatorInfoTest, HandleOLP) {
  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "# No provider entry.\n"
      "olp:,,\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "# Badly formed APN\n"
      "olp:,\n");
  EXPECT_FALSE(info_.Load(info_file_path_));
  EXPECT_TRUE(info_.operators().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "# Badly formed APN\n"
      "olp:,,\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(1, info_.operators()[0]->olp_list().size());
  EXPECT_TRUE(info_.operators()[0]->olp_list()[0]->GetURL().empty());
  EXPECT_TRUE(info_.operators()[0]->olp_list()[0]->GetMethod().empty());
  EXPECT_TRUE(info_.operators()[0]->olp_list()[0]->GetPostData().empty());

  TruncateAndWriteToFile(
      "serviceproviders:3.0\n"
      "provider:1,1,0,0\n"
      "# Badly formed APN\n"
      "olp:a,b,c\n"
      "olp:d,e,f\n");
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(1, info_.operators().size());
  EXPECT_EQ(2, info_.operators()[0]->olp_list().size());
  EXPECT_EQ("a", info_.operators()[0]->olp_list()[0]->GetMethod());
  EXPECT_EQ("b", info_.operators()[0]->olp_list()[0]->GetURL());
  EXPECT_EQ("c", info_.operators()[0]->olp_list()[0]->GetPostData());
  EXPECT_EQ("d", info_.operators()[0]->olp_list()[1]->GetMethod());
  EXPECT_EQ("e", info_.operators()[0]->olp_list()[1]->GetURL());
  EXPECT_EQ("f", info_.operators()[0]->olp_list()[1]->GetPostData());
}

}  // namespace shill

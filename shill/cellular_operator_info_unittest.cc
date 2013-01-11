// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_operator_info.h"

#include <base/file_util.h>
#include <gtest/gtest.h>

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
    ASSERT_TRUE(file_util::CreateTemporaryFile(&info_file_path_));
    ASSERT_EQ(arraysize(kTestInfoFileContent),
              file_util::WriteFile(info_file_path_, kTestInfoFileContent,
                                   arraysize(kTestInfoFileContent)));
  }

  void TearDown() {
    ASSERT_TRUE(file_util::Delete(info_file_path_, false));
  }

  GLib glib_;
  FilePath info_file_path_;
  CellularOperatorInfo info_;
};

TEST_F(CellularOperatorInfoTest, ParseSuccess) {
  EXPECT_TRUE(info_.Load(info_file_path_));
  EXPECT_EQ(info_.operators().size(), 2);

  const CellularOperatorInfo::CellularOperator *provider = info_.operators()[0];
  EXPECT_FALSE(provider->is_primary());
  EXPECT_TRUE(provider->requires_roaming());
  EXPECT_EQ(provider->country(), "us");
  EXPECT_EQ(provider->name_list().size(), 1);
  EXPECT_EQ(provider->name_list()[0].language, "");
  EXPECT_EQ(provider->name_list()[0].name, "TestProvider1");
  EXPECT_EQ(provider->mccmnc_list().size(), 2);
  EXPECT_EQ(provider->mccmnc_list()[0], "000001");
  EXPECT_EQ(provider->mccmnc_list()[1], "000002");
  EXPECT_EQ(provider->sid_list().size(), 3);
  EXPECT_EQ(provider->sid_list()[0], "1");
  EXPECT_EQ(provider->sid_list()[1], "2");
  EXPECT_EQ(provider->sid_list()[2], "3");
  EXPECT_EQ(provider->olp_list().size(), 1);
  EXPECT_EQ(provider->olp_list()[0]->GetURL(), "https://testurl");
  EXPECT_EQ(provider->olp_list()[0]->GetMethod(), "POST");
  EXPECT_EQ(provider->olp_list()[0]->GetPostData(),
            "imei=${imei}&iccid=${iccid}");
  EXPECT_EQ(provider->apn_list().size(), 1);
  EXPECT_EQ(provider->apn_list()[0]->apn(), "testprovider1apn");
  EXPECT_EQ(provider->apn_list()[0]->username(), "");
  EXPECT_EQ(provider->apn_list()[0]->password(), "");
  EXPECT_EQ(provider->apn_list()[0]->name_list().size(), 2);
  EXPECT_EQ(provider->apn_list()[0]->name_list()[0].language, "en");
  EXPECT_EQ(provider->apn_list()[0]->name_list()[0].name, "Test Provider 1");
  EXPECT_EQ(provider->apn_list()[0]->name_list()[1].language, "de");
  EXPECT_EQ(provider->apn_list()[0]->name_list()[1].name,
            "Testmobilfunkanbieter 1");

  const CellularOperatorInfo::CellularOperator *provider2 =
      info_.operators()[1];
  EXPECT_TRUE(provider2->is_primary());
  EXPECT_FALSE(provider2->requires_roaming());
  EXPECT_EQ(provider2->country(), "us");
  EXPECT_EQ(provider2->name_list().size(), 2);
  EXPECT_EQ(provider2->name_list()[0].language, "");
  EXPECT_EQ(provider2->name_list()[0].name, "TestProviderTwo");
  EXPECT_EQ(provider2->name_list()[1].language, "");
  EXPECT_EQ(provider2->name_list()[1].name, "TestProvider2");
  EXPECT_EQ(provider2->mccmnc_list().size(), 2);
  EXPECT_EQ(provider2->mccmnc_list()[0], "100001");
  EXPECT_EQ(provider2->mccmnc_list()[1], "100002");
  EXPECT_EQ(provider2->sid_list().size(), 2);
  EXPECT_EQ(provider2->sid_list()[0], "4");
  EXPECT_EQ(provider2->sid_list()[1], "5");
  EXPECT_EQ(provider2->olp_list().size(), 2);
  EXPECT_EQ(provider2->olp_list()[0]->GetURL(), "https://testurl2");
  EXPECT_EQ(provider2->olp_list()[0]->GetMethod(), "");
  EXPECT_EQ(provider2->olp_list()[0]->GetPostData(), "");
  EXPECT_EQ(provider2->olp_list()[1]->GetURL(), "https://testurl3");
  EXPECT_EQ(provider2->olp_list()[1]->GetMethod(), "");
  EXPECT_EQ(provider2->olp_list()[1]->GetPostData(), "");
  EXPECT_EQ(provider2->apn_list().size(), 2);
  EXPECT_EQ(provider2->apn_list()[0]->apn(), "testprovider2apn");
  EXPECT_EQ(provider2->apn_list()[0]->username(), "");
  EXPECT_EQ(provider2->apn_list()[0]->password(), "");
  EXPECT_EQ(provider2->apn_list()[0]->name_list().size(), 1);
  EXPECT_EQ(provider2->apn_list()[0]->name_list()[0].language, "");
  EXPECT_EQ(provider2->apn_list()[0]->name_list()[0].name, "Test Provider 2");
  EXPECT_EQ(provider2->apn_list()[1]->apn(), "testprovider2apn2");
  EXPECT_EQ(provider2->apn_list()[1]->username(), "testusername");
  EXPECT_EQ(provider2->apn_list()[1]->password(), "testpassword");
  EXPECT_EQ(provider2->apn_list()[1]->name_list().size(), 1);
  EXPECT_EQ(provider2->apn_list()[1]->name_list()[0].language, "tr");
  EXPECT_EQ(provider2->apn_list()[1]->name_list()[0].name, "Test Operatoru 2");
}

TEST_F(CellularOperatorInfoTest, GetCellularOperatorByMCCMNC) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("1"));
  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("000003"));
  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("bananas"));
  EXPECT_FALSE(info_.GetCellularOperatorByMCCMNC("abcd"));

  const CellularOperatorInfo::CellularOperator *provider = NULL;
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("000001")));
  EXPECT_EQ(provider, info_.operators()[0]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("100001")));
  EXPECT_EQ(provider, info_.operators()[1]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("000002")));
  EXPECT_EQ(provider, info_.operators()[0]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorByMCCMNC("100002")));
  EXPECT_EQ(provider, info_.operators()[1]);
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
  EXPECT_EQ(provider, info_.operators()[0]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("4")));
  EXPECT_EQ(provider, info_.operators()[1]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("2")));
  EXPECT_EQ(provider, info_.operators()[0]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("5")));
  EXPECT_EQ(provider, info_.operators()[1]);
  EXPECT_TRUE((provider = info_.GetCellularOperatorBySID("3")));
  EXPECT_EQ(provider, info_.operators()[0]);
}

TEST_F(CellularOperatorInfoTest, GetCellularOperatorsByName) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  const vector<const CellularOperatorInfo::CellularOperator *> *list = NULL;
  EXPECT_FALSE(info_.GetCellularOperatorsByName("banana"));
  EXPECT_FALSE(info_.GetCellularOperatorsByName("TestProvider2"));

  EXPECT_TRUE((list = info_.GetCellularOperatorsByName("TestProvider1")));
  EXPECT_EQ(list->size(), 1);
  EXPECT_EQ((*list)[0]->apn_list()[0]->apn(), "testprovider1apn");

  EXPECT_FALSE(info_.GetCellularOperatorsByName("TestProvider2"));

  EXPECT_TRUE((list = info_.GetCellularOperatorsByName("TestProviderTwo")));
  EXPECT_EQ(list->size(), 1);
  EXPECT_EQ((*list)[0]->apn_list()[0]->apn(), "testprovider2apn");
}

TEST_F(CellularOperatorInfoTest, GetOLPByMCCMNC) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  const CellularService::OLP *olp = NULL;
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
  EXPECT_EQ("", olp->GetMethod());
  EXPECT_EQ("", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPByMCCMNC("100002")));
  EXPECT_EQ("https://testurl2", olp->GetURL());
  EXPECT_EQ("", olp->GetMethod());
  EXPECT_EQ("", olp->GetPostData());

  EXPECT_FALSE(info_.GetOLPByMCCMNC("000003"));
  EXPECT_FALSE(info_.GetOLPByMCCMNC("000004"));
}

TEST_F(CellularOperatorInfoTest, GetOLPBySID) {
  EXPECT_TRUE(info_.Load(info_file_path_));

  const CellularService::OLP *olp = NULL;
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
  EXPECT_EQ("", olp->GetMethod());
  EXPECT_EQ("", olp->GetPostData());

  EXPECT_TRUE((olp = info_.GetOLPBySID("5")));
  EXPECT_EQ("https://testurl3", olp->GetURL());
  EXPECT_EQ("", olp->GetMethod());
  EXPECT_EQ("", olp->GetPostData());

  EXPECT_FALSE(info_.GetOLPBySID("6"));
  EXPECT_FALSE(info_.GetOLPBySID("7"));
}

TEST_F(CellularOperatorInfoTest, HandleProvider) {
  CellularOperatorInfo::ParserState state;
  EXPECT_FALSE(info_.HandleProvider(&state, "0,0,0"));
  EXPECT_FALSE(state.provider);
  EXPECT_EQ(info_.operators_.size(), 0);

  EXPECT_TRUE(info_.HandleProvider(&state, "0,0,0,0"));
  EXPECT_TRUE(state.provider);
  EXPECT_EQ(info_.operators_.size(), 1);
  EXPECT_FALSE(state.provider->is_primary());
  EXPECT_FALSE(state.provider->requires_roaming());

  EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,1"));
  EXPECT_EQ(info_.operators_.size(), 2);
  EXPECT_EQ(state.provider, info_.operators_[1]);
  EXPECT_FALSE(state.provider->is_primary());
  EXPECT_TRUE(state.provider->requires_roaming());
}

TEST_F(CellularOperatorInfoTest, HandleMCCMNC) {
  CellularOperatorInfo::ParserState state;
  EXPECT_FALSE(info_.HandleMCCMNC(&state, "1,1"));

  EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,0"));
  EXPECT_TRUE(state.provider);

  EXPECT_FALSE(info_.HandleMCCMNC(&state, ""));
  EXPECT_TRUE(state.provider->mccmnc_list().empty());
  EXPECT_TRUE(info_.mccmnc_to_operator_.empty());

  EXPECT_FALSE(info_.HandleMCCMNC(&state, "000001,0,000002"));
  EXPECT_TRUE(state.provider->mccmnc_list().empty());
  EXPECT_TRUE(info_.mccmnc_to_operator_.empty());

  EXPECT_TRUE(info_.HandleMCCMNC(&state, "000001,0,000002,3"));
  EXPECT_EQ(state.provider->mccmnc_list().size(), 2);
  EXPECT_EQ(info_.mccmnc_to_operator_.size(), 2);
  EXPECT_EQ(info_.mccmnc_to_operator_["000001"], state.provider);
  EXPECT_EQ(info_.mccmnc_to_operator_["000002"], state.provider);
  EXPECT_EQ(state.provider->mccmnc_list()[0], "000001");
  EXPECT_EQ(state.provider->mccmnc_list()[1], "000002");
  EXPECT_EQ(state.provider->mccmnc_to_olp_idx_.size(), 2);
  EXPECT_EQ(state.provider->mccmnc_to_olp_idx_["000001"], 0);
  EXPECT_EQ(state.provider->mccmnc_to_olp_idx_["000002"], 3);
}

TEST_F(CellularOperatorInfoTest, HandleName) {
  CellularOperatorInfo::ParserState state;
  EXPECT_FALSE(info_.HandleName(&state, ","));

  EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,0"));
  EXPECT_TRUE(state.provider);
  EXPECT_TRUE(state.provider->name_list().empty());

  EXPECT_FALSE(info_.HandleName(&state, ",,"));
  EXPECT_TRUE(state.provider->name_list().empty());

  EXPECT_TRUE(info_.HandleName(&state, "en,Test Name"));
  EXPECT_EQ(state.provider->name_list().size(), 1);

  const CellularOperatorInfo::LocalizedName &name =
      state.provider->name_list()[0];
  EXPECT_EQ(name.language, "en");
  EXPECT_EQ(name.name, "Test Name");

  EXPECT_TRUE(info_.HandleName(&state, ",Other Test Name"));
  EXPECT_EQ(state.provider->name_list().size(), 2);

  const CellularOperatorInfo::LocalizedName &name2 =
      state.provider->name_list()[1];
  EXPECT_EQ(name2.language, "");
  EXPECT_EQ(name2.name, "Other Test Name");
}

TEST_F(CellularOperatorInfoTest, HandleAPN) {
  CellularOperatorInfo::ParserState state;
  EXPECT_FALSE(info_.HandleAPN(&state, ",,,"));

  EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,0"));
  EXPECT_TRUE(state.provider);
  EXPECT_TRUE(state.provider->apn_list().empty());

  EXPECT_FALSE(info_.HandleAPN(&state, ","));
  EXPECT_TRUE(state.provider->apn_list().empty());

  EXPECT_FALSE(state.parsing_apn);

  EXPECT_TRUE(info_.HandleAPN(&state, "0,testapn,testusername,testpassword"));
  EXPECT_EQ(state.provider->apn_list().size(), 1);
  EXPECT_TRUE(state.parsing_apn);

  const CellularOperatorInfo::MobileAPN *apn = state.provider->apn_list()[0];
  EXPECT_EQ(apn->apn(), "testapn");
  EXPECT_EQ(apn->username(), "testusername");
  EXPECT_EQ(apn->password(), "testpassword");
}

TEST_F(CellularOperatorInfoTest, HandleAPNName) {
  CellularOperatorInfo::ParserState state;
  EXPECT_FALSE(info_.HandleAPNName(&state, ","));
  state.parsing_apn = true;
  EXPECT_FALSE(info_.HandleAPNName(&state, ","));
  state.parsing_apn = false;
  state.apn = (CellularOperatorInfo::MobileAPN *)1;
  EXPECT_FALSE(info_.HandleAPNName(&state, ","));

  EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,0"));
  EXPECT_TRUE(state.provider);
  EXPECT_TRUE(info_.HandleAPN(&state, ",,,"));
  EXPECT_TRUE(state.parsing_apn && state.apn);

  const CellularOperatorInfo::MobileAPN *apn = state.provider->apn_list()[0];

  EXPECT_FALSE(info_.HandleAPNName(&state, ",,"));
  EXPECT_EQ(apn->name_list().size(), 0);

  EXPECT_TRUE(info_.HandleAPNName(&state, "en,APN Name"));
  EXPECT_EQ(apn->name_list().size(), 1);

  const CellularOperatorInfo::LocalizedName &name = apn->name_list()[0];
  EXPECT_EQ(name.language, "en");
  EXPECT_EQ(name.name, "APN Name");

  EXPECT_TRUE(info_.HandleAPNName(&state, ",Other APN Name"));
  EXPECT_EQ(apn->name_list().size(), 2);

  const CellularOperatorInfo::LocalizedName &name2 = apn->name_list()[1];
  EXPECT_EQ(name2.language, "");
  EXPECT_EQ(name2.name, "Other APN Name");
}

TEST_F(CellularOperatorInfoTest, HandleSID) {
 CellularOperatorInfo::ParserState state;
 EXPECT_FALSE(info_.HandleSID(&state, "1,0"));

 EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,0"));
 EXPECT_TRUE(state.provider);

 EXPECT_FALSE(info_.HandleSID(&state, ""));
 EXPECT_TRUE(state.provider->sid_list().empty());
 EXPECT_TRUE(info_.sid_to_operator_.empty());

 EXPECT_FALSE(info_.HandleSID(&state, "1,2,3"));
 EXPECT_TRUE(state.provider->sid_list().empty());
 EXPECT_TRUE(info_.sid_to_operator_.empty());

 EXPECT_TRUE(info_.HandleSID(&state, "1,5,2,3,3,0"));
 EXPECT_EQ(state.provider->sid_list().size(), 3);
 EXPECT_EQ(info_.sid_to_operator_.size(), 3);
 EXPECT_EQ(info_.sid_to_operator_["1"], state.provider);
 EXPECT_EQ(info_.sid_to_operator_["2"], state.provider);
 EXPECT_EQ(info_.sid_to_operator_["3"], state.provider);
 EXPECT_EQ(state.provider->sid_list()[0], "1");
 EXPECT_EQ(state.provider->sid_list()[1], "2");
 EXPECT_EQ(state.provider->sid_list()[2], "3");
 EXPECT_EQ(state.provider->sid_to_olp_idx_.size(), 3);
 EXPECT_EQ(state.provider->sid_to_olp_idx_["1"], 5);
 EXPECT_EQ(state.provider->sid_to_olp_idx_["2"], 3);
 EXPECT_EQ(state.provider->sid_to_olp_idx_["3"], 0);
}

TEST_F(CellularOperatorInfoTest, HandleOLP) {
  CellularOperatorInfo::ParserState state;
  EXPECT_FALSE(info_.HandleOLP(&state, ",,"));

  EXPECT_TRUE(info_.HandleProvider(&state, "1,1,0,0"));
  EXPECT_TRUE(state.provider);
  EXPECT_EQ(0, state.provider->olp_list().size());

  EXPECT_FALSE(info_.HandleOLP(&state, ","));
  EXPECT_EQ(0, state.provider->olp_list().size());

  EXPECT_TRUE(info_.HandleOLP(&state, ",,"));
  EXPECT_EQ(1, state.provider->olp_list().size());
  EXPECT_EQ(state.provider->olp_list()[0]->GetURL(), "");
  EXPECT_EQ(state.provider->olp_list()[0]->GetMethod(), "");
  EXPECT_EQ(state.provider->olp_list()[0]->GetPostData(), "");

  EXPECT_TRUE(info_.HandleOLP(&state, "a,b,c"));
  EXPECT_EQ(2, state.provider->olp_list().size());
  EXPECT_EQ(state.provider->olp_list()[0]->GetURL(), "");
  EXPECT_EQ(state.provider->olp_list()[0]->GetMethod(), "");
  EXPECT_EQ(state.provider->olp_list()[0]->GetPostData(), "");
  EXPECT_EQ(state.provider->olp_list()[1]->GetMethod(), "a");
  EXPECT_EQ(state.provider->olp_list()[1]->GetURL(), "b");
  EXPECT_EQ(state.provider->olp_list()[1]->GetPostData(), "c");
}

TEST_F(CellularOperatorInfoTest, ParseNameLine) {
  CellularOperatorInfo::LocalizedName name;
  EXPECT_FALSE(info_.ParseNameLine(",,", &name));
  EXPECT_FALSE(info_.ParseNameLine("", &name));
  EXPECT_TRUE(info_.ParseNameLine("a,b", &name));
  EXPECT_EQ(name.language, "a");
  EXPECT_EQ(name.name, "b");
}

TEST_F(CellularOperatorInfoTest, ParseKeyValue) {
  string line = "key:value";
  string key, value;
  EXPECT_TRUE(CellularOperatorInfo::ParseKeyValue(line, ':', &key, &value));
  EXPECT_EQ("key", key);
  EXPECT_EQ("value", value);

  line = "key:::::";
  EXPECT_TRUE(CellularOperatorInfo::ParseKeyValue(line, ':', &key, &value));
  EXPECT_EQ("key", key);
  EXPECT_EQ("::::", value);

  line = ":";
  EXPECT_TRUE(CellularOperatorInfo::ParseKeyValue(line, ':', &key, &value));
  EXPECT_EQ("", key);
  EXPECT_EQ("", value);

  line = ":value";
  EXPECT_TRUE(CellularOperatorInfo::ParseKeyValue(line, ':', &key, &value));
  EXPECT_EQ("", key);
  EXPECT_EQ("value", value);

  line = "";
  EXPECT_FALSE(CellularOperatorInfo::ParseKeyValue(line, ':', &key, &value));
}

}  // namespace shill

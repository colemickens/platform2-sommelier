// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mobile_operator.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "shill/cellular_service.h"  // TODO(armansito): Remove once OLP moves.
#include "shill/mock_cellular_operator_info.h"
#include "shill/mock_modem_info.h"

using testing::_;
using testing::Mock;
using testing::Return;

namespace shill {

namespace {

const char kTestMobileProviderDbPath[] = "provider_db_unittest.bfd";
const char kTestUnknownOperatorCode[] = "unknown-code";
const char kTestUnknownOperatorName[] = "unknown-name";
const char kTestOperatorInfoCodeMccmnc[] = "test-mccmnc";
const char kTestOperatorInfoCodeSid[] = "test-sid";
const char kTestNoOlpOperatorInfoCode[] = "no-olp-code";
const char kTestOperatorInfoName[] = "test-operator-name";
const char kTestOperatorInfoCountry[] = "test-country";
const char kTestOlpUrl[] = "test-url";
const char kTestOlpMethod[] = "test-method";
const char kTestOlpPostData[] = "test-post-data";
const char kTestLocalizedName[] = "test-localized-name";
const char kTestLocalizedNameLanguage[] = "test-localized-name-language";
const char kTestApn0[] = "test-apn0";
const char kTestApnUsername0[] = "test-apn-username0";
const char kTestApnPassword0[] = "test-apn-password0";
const char kTestApn1[] = "test-apn1";
const char kTestApnUsername1[] = "test-apn-username1";
const char kTestApnPassword1[] = "test-apn-password1";

const char kProviderDbCode0[] = "22803";
const char kProviderDbName0[] = "Orange";
const char kProviderDbCountry0[] = "ch";
const char kProviderDbCode1[] = "310038";
const char kProviderDbName1[] = "AT&T";
const char kProviderDbCountry1[] = "us";

}  // namespace

class TestMobileOperatorObserver : public MobileOperator::Observer {
 public:
  MOCK_METHOD1(OnHomeProviderInfoChanged, void(const MobileOperator *));
  MOCK_METHOD1(OnServingOperatorInfoChanged, void(const MobileOperator *));
  MOCK_METHOD1(OnApnListChanged, void(const MobileOperator *));
  MOCK_METHOD1(OnOnlinePaymentUrlTemplateChanged,
               void(const MobileOperator *));
};

class MobileOperatorTest : public testing::Test {
 public:
  MobileOperatorTest()
    : modem_info_(NULL, NULL, NULL, NULL, NULL),
      operator_(&modem_info_) {}

  void SetUp() {
    operator_.AddObserver(&observer_);
    modem_info_.SetProviderDB(kTestMobileProviderDbPath);
    SetupCellularOperatorInfo();
  }

  void TearDown() {
    operator_.RemoveObserver(&observer_);
  }

  // Called from SetUp.
  void SetupCellularOperatorInfo() {
    test_olp_.SetURL(kTestOlpUrl);
    test_olp_.SetMethod(kTestOlpMethod);
    test_olp_.SetPostData(kTestOlpPostData);
    CellularOperatorInfo::LocalizedName name;
    name.name = kTestOperatorInfoName;
    test_operator_.name_list_.push_back(name);
    test_operator_.country_ = kTestOperatorInfoCountry;
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetCellularOperatorByMCCMNC(_))
      .WillByDefault(Return(nullptr));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetCellularOperatorByMCCMNC(kTestOperatorInfoCodeMccmnc))
      .WillByDefault(Return(&test_operator_));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetCellularOperatorByMCCMNC(kTestNoOlpOperatorInfoCode))
      .WillByDefault(Return(&test_operator_));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetCellularOperatorBySID(_))
      .WillByDefault(Return(nullptr));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetCellularOperatorBySID(kTestOperatorInfoCodeSid))
      .WillByDefault(Return(&test_operator_));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetCellularOperatorBySID(kTestNoOlpOperatorInfoCode))
      .WillByDefault(Return(&test_operator_));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetOLPByMCCMNC(_))
      .WillByDefault(Return(nullptr));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetOLPByMCCMNC(kTestOperatorInfoCodeMccmnc))
      .WillByDefault(Return(&test_olp_));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetOLPBySID(_))
      .WillByDefault(Return(nullptr));
    ON_CALL(*modem_info_.mock_cellular_operator_info(),
            GetOLPBySID(kTestOperatorInfoCodeSid))
      .WillByDefault(Return(&test_olp_));
  }

  // Called from tests.
  void SetupApnList() {
    CellularOperatorInfo::LocalizedName localized_name(
        kTestLocalizedName, kTestLocalizedNameLanguage);
    CellularOperatorInfo::MobileAPN *apn = new CellularOperatorInfo::MobileAPN;
    apn->apn = kTestApn0;
    apn->username = kTestApnUsername0;
    apn->password = kTestApnPassword0;
    apn->name_list.push_back(localized_name);
    test_operator_.apn_list_.push_back(apn);  // Passes ownership.

    apn = new CellularOperatorInfo::MobileAPN;
    apn->apn = kTestApn1;
    apn->username = kTestApnUsername1;
    apn->password = kTestApnPassword1;
    apn->name_list.push_back(CellularOperatorInfo::LocalizedName(
        kTestOperatorInfoName, ""));
    apn->name_list.push_back(localized_name);
    test_operator_.apn_list_.push_back(apn);  // Passes ownership.
  }

 protected:
  MockModemInfo modem_info_;
  MobileOperator operator_;
  TestMobileOperatorObserver observer_;
  CellularOperatorInfo::CellularOperator test_operator_;
  CellularService::OLP test_olp_;
};

TEST_F(MobileOperatorTest, OtaOperatorInfoReceivedNotFound) {
  EXPECT_TRUE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.apn_list().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());

  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);

  operator_.OtaOperatorInfoReceived(
      "", "", MobileOperator::kOperatorCodeTypeMCCMNC);
  operator_.OtaOperatorInfoReceived(
      "", "", MobileOperator::kOperatorCodeTypeSID);
  Mock::VerifyAndClearExpectations(&observer_);

  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(2);
  operator_.OtaOperatorInfoReceived(kTestUnknownOperatorCode, "",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_EQ(kTestUnknownOperatorCode,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(operator_.serving_operator().end(),
            operator_.serving_operator().find("name"));
  operator_.OtaOperatorInfoReceived("", kTestUnknownOperatorName,
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  EXPECT_EQ(operator_.serving_operator().end(),
            operator_.serving_operator().find("code"));
  EXPECT_EQ(kTestUnknownOperatorName,
            operator_.serving_operator().find("name")->second);

  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.apn_list().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
}

TEST_F(MobileOperatorTest, SimOperatorInfoReceivedNotFound) {
  EXPECT_TRUE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.apn_list().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());

  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);

  operator_.SimOperatorInfoReceived("", "");
  Mock::VerifyAndClearExpectations(&observer_);

  operator_.SimOperatorInfoReceived(kTestUnknownOperatorCode, "");
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_EQ(kTestUnknownOperatorCode,
            operator_.home_provider().find("code")->second);
  EXPECT_EQ(operator_.home_provider().end(),
            operator_.home_provider().find("name"));

  operator_.SimOperatorInfoReceived("", kTestUnknownOperatorName);
  EXPECT_EQ(operator_.home_provider().end(),
            operator_.home_provider().find("code"));
  EXPECT_EQ(kTestUnknownOperatorName,
            operator_.home_provider().find("name")->second);

  EXPECT_TRUE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.apn_list().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
}

TEST_F(MobileOperatorTest, OtaOperatorInfoReceivedMccmnc) {
  // Operator code only.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kTestNoOlpOperatorInfoCode, "",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestNoOlpOperatorInfoCode,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(kTestOperatorInfoCountry,
            operator_.serving_operator().find("country")->second);

  // Update to same value.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  operator_.OtaOperatorInfoReceived(kTestNoOlpOperatorInfoCode,
                                    kTestOperatorInfoName,
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestNoOlpOperatorInfoCode,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(kTestOperatorInfoCountry,
            operator_.serving_operator().find("country")->second);

  // Update With OLP.
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kTestOperatorInfoCodeMccmnc, "",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_EQ(kTestOperatorInfoCodeMccmnc,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.serving_operator().find("name")->second);
  EXPECT_FALSE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestOlpUrl,
            operator_.online_payment_url_template().find("url")->second);
  EXPECT_EQ(kTestOlpMethod,
            operator_.online_payment_url_template().find("method")->second);
  EXPECT_EQ(kTestOlpPostData,
            operator_.online_payment_url_template().find("postdata")->second);

  // Update just the operator name.
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  operator_.OtaOperatorInfoReceived(kTestOperatorInfoCodeMccmnc, "banana",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_EQ(kTestOperatorInfoCodeMccmnc,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ("banana",
            operator_.serving_operator().find("name")->second);
  EXPECT_FALSE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestOlpUrl,
            operator_.online_payment_url_template().find("url")->second);
  EXPECT_EQ(kTestOlpMethod,
            operator_.online_payment_url_template().find("method")->second);
  EXPECT_EQ(kTestOlpPostData,
            operator_.online_payment_url_template().find("postdata")->second);
}

TEST_F(MobileOperatorTest, OtaOperatorInfoReceivedSid) {
  // Operator code only.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kTestNoOlpOperatorInfoCode, "",
                                    MobileOperator::kOperatorCodeTypeSID);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestNoOlpOperatorInfoCode,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(kTestOperatorInfoCountry,
            operator_.serving_operator().find("country")->second);

  // Update to same value.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  operator_.OtaOperatorInfoReceived(kTestNoOlpOperatorInfoCode,
                                    kTestOperatorInfoName,
                                    MobileOperator::kOperatorCodeTypeSID);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestNoOlpOperatorInfoCode,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(kTestOperatorInfoCountry,
            operator_.serving_operator().find("country")->second);

  // Update With OLP.
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kTestOperatorInfoCodeSid, "",
                                    MobileOperator::kOperatorCodeTypeSID);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_EQ(kTestOperatorInfoCodeSid,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.serving_operator().find("name")->second);
  EXPECT_FALSE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestOlpUrl,
            operator_.online_payment_url_template().find("url")->second);
  EXPECT_EQ(kTestOlpMethod,
            operator_.online_payment_url_template().find("method")->second);
  EXPECT_EQ(kTestOlpPostData,
            operator_.online_payment_url_template().find("postdata")->second);

  // Update just the operator name.
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  operator_.OtaOperatorInfoReceived(kTestOperatorInfoCodeSid, "banana",
                                    MobileOperator::kOperatorCodeTypeSID);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_EQ(kTestOperatorInfoCodeSid,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ("banana",
            operator_.serving_operator().find("name")->second);
  EXPECT_FALSE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestOlpUrl,
            operator_.online_payment_url_template().find("url")->second);
  EXPECT_EQ(kTestOlpMethod,
            operator_.online_payment_url_template().find("method")->second);
  EXPECT_EQ(kTestOlpPostData,
            operator_.online_payment_url_template().find("postdata")->second);
}

TEST_F(MobileOperatorTest, OtaReceivedMobileProviderDb) {
  // Update with operator ID that doesn't exist in COI but is in
  // the mobile provider db.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kProviderDbCode0, "",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kProviderDbCode0,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kProviderDbName0,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(kProviderDbCountry0,
            operator_.serving_operator().find("country")->second);

  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kProviderDbCode1, "",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kProviderDbCode1,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(kProviderDbName1,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(kProviderDbCountry1,
            operator_.serving_operator().find("country")->second);

  // Look up by name.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived("", kProviderDbName0,
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(operator_.serving_operator().end(),
            operator_.serving_operator().find("code"));
  EXPECT_EQ(kProviderDbName0,
            operator_.serving_operator().find("name")->second);
  EXPECT_EQ(operator_.serving_operator().end(),
            operator_.serving_operator().find("country"));

  // mobile provider DB doesn't support SID.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(1);
  operator_.OtaOperatorInfoReceived(kProviderDbCode0, "",
                                    MobileOperator::kOperatorCodeTypeSID);
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kProviderDbCode0,
            operator_.serving_operator().find("code")->second);
  EXPECT_EQ(operator_.serving_operator().end(),
            operator_.serving_operator().find("name"));
  EXPECT_EQ(operator_.serving_operator().end(),
            operator_.serving_operator().find("country"));
}

TEST_F(MobileOperatorTest, SimOperatorInfoReceived) {
  // Operator code only.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  operator_.SimOperatorInfoReceived(kTestNoOlpOperatorInfoCode, "");
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestNoOlpOperatorInfoCode,
            operator_.home_provider().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.home_provider().find("name")->second);
  EXPECT_EQ(kTestOperatorInfoCountry,
            operator_.home_provider().find("country")->second);

  // Update to same value.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  operator_.SimOperatorInfoReceived(kTestNoOlpOperatorInfoCode, "");
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestNoOlpOperatorInfoCode,
            operator_.home_provider().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.home_provider().find("name")->second);
  EXPECT_EQ(kTestOperatorInfoCountry,
            operator_.home_provider().find("country")->second);

  // Update with OLP: OLP shouldn't change.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  operator_.SimOperatorInfoReceived(kTestOperatorInfoCodeMccmnc, "");
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestOperatorInfoCodeMccmnc,
            operator_.home_provider().find("code")->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.home_provider().find("name")->second);

  // Update just the operator name.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  operator_.SimOperatorInfoReceived(kTestOperatorInfoCodeMccmnc, "banana");
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_EQ(kTestOperatorInfoCodeMccmnc,
            operator_.home_provider().find("code")->second);
  EXPECT_EQ("banana",
            operator_.home_provider().find("name")->second);
  EXPECT_TRUE(operator_.online_payment_url_template().empty());

  // Try passing a SID. No matching entry will be found.
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(1);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  operator_.SimOperatorInfoReceived(kTestOperatorInfoCodeSid, "");
  Mock::VerifyAndClearExpectations(&observer_);
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_EQ(kTestOperatorInfoCodeSid,
            operator_.home_provider().find("code")->second);
  EXPECT_EQ(operator_.home_provider().end(),
            operator_.home_provider().find("name"));
  EXPECT_EQ(operator_.home_provider().end(),
            operator_.home_provider().find("country"));
}

TEST_F(MobileOperatorTest, ApnListUpdate) {
  SetupApnList();
  EXPECT_TRUE(operator_.apn_list().empty());
  EXPECT_TRUE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());

  // Set MCCMNC OTA.
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(3);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(3);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(2);
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(0);
  operator_.OtaOperatorInfoReceived(kTestOperatorInfoCodeMccmnc, "",
                                    MobileOperator::kOperatorCodeTypeMCCMNC);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_FALSE(operator_.online_payment_url_template().empty());
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_EQ(2, operator_.apn_list().size());
  EXPECT_EQ(kTestApn0, operator_.apn_list()[0].find(kApnProperty)->second);
  EXPECT_EQ(kTestApnUsername0,
            operator_.apn_list()[0].find(kApnUsernameProperty)->second);
  EXPECT_EQ(kTestApnPassword0,
            operator_.apn_list()[0].find(kApnPasswordProperty)->second);
  EXPECT_EQ(kTestLocalizedName,
            operator_.apn_list()[0].find(kApnLocalizedNameProperty)->second);
  EXPECT_EQ(kTestLocalizedNameLanguage,
            operator_.apn_list()[0].find(kApnLanguageProperty)->second);
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnNameProperty));

  EXPECT_EQ(kTestApn1, operator_.apn_list()[1].find("apn")->second);
  EXPECT_EQ(kTestApnUsername1,
            operator_.apn_list()[1].find(kApnUsernameProperty)->second);
  EXPECT_EQ(kTestApnPassword1,
            operator_.apn_list()[1].find(kApnPasswordProperty)->second);
  EXPECT_EQ(kTestLocalizedName,
            operator_.apn_list()[1].find(kApnLocalizedNameProperty)->second);
  EXPECT_EQ(kTestLocalizedNameLanguage,
            operator_.apn_list()[1].find(kApnLanguageProperty)->second);
  EXPECT_EQ(kTestOperatorInfoName,
            operator_.apn_list()[1].find(kApnNameProperty)->second);

  // Update APN list via provider DB.
  operator_.OtaOperatorInfoReceived(
      "22801", "", MobileOperator::kOperatorCodeTypeMCCMNC);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_TRUE(operator_.home_provider().empty());
  EXPECT_EQ(1, operator_.apn_list().size());
  EXPECT_EQ("gprs.swisscom.ch",
            operator_.apn_list()[0].find(kApnProperty)->second);
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnUsernameProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnPasswordProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnLocalizedNameProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnLanguageProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnNameProperty));

  operator_.OtaOperatorInfoReceived(
      "310160", "", MobileOperator::kOperatorCodeTypeMCCMNC);
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_TRUE(operator_.home_provider().empty());

  EXPECT_EQ(4, operator_.apn_list().size());
  EXPECT_EQ("epc.tmobile.com",
            operator_.apn_list()[0].find(kApnProperty)->second);
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnUsernameProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnPasswordProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnLocalizedNameProperty));
  EXPECT_EQ(operator_.apn_list()[0].end(),
            operator_.apn_list()[0].find(kApnLanguageProperty));
  EXPECT_EQ("Internet/WebConnect",
            operator_.apn_list()[0].find(kApnNameProperty)->second);

  EXPECT_EQ("wap.voicestream.com",
            operator_.apn_list()[1].find(kApnProperty)->second);
  EXPECT_EQ(operator_.apn_list()[1].end(),
            operator_.apn_list()[1].find(kApnUsernameProperty));
  EXPECT_EQ(operator_.apn_list()[1].end(),
            operator_.apn_list()[1].find(kApnPasswordProperty));
  EXPECT_EQ(operator_.apn_list()[1].end(),
            operator_.apn_list()[1].find(kApnLocalizedNameProperty));
  EXPECT_EQ(operator_.apn_list()[1].end(),
            operator_.apn_list()[1].find(kApnLanguageProperty));
  EXPECT_EQ("Web2Go/t-zones",
            operator_.apn_list()[1].find(kApnNameProperty)->second);

  EXPECT_EQ("internet2.voicestream.com",
            operator_.apn_list()[2].find(kApnProperty)->second);
  EXPECT_EQ(operator_.apn_list()[2].end(),
            operator_.apn_list()[2].find(kApnUsernameProperty));
  EXPECT_EQ(operator_.apn_list()[2].end(),
            operator_.apn_list()[2].find(kApnPasswordProperty));
  EXPECT_EQ(operator_.apn_list()[2].end(),
            operator_.apn_list()[2].find(kApnLocalizedNameProperty));
  EXPECT_EQ(operator_.apn_list()[2].end(),
            operator_.apn_list()[2].find(kApnLanguageProperty));
  EXPECT_EQ("Internet (old)",
            operator_.apn_list()[2].find(kApnNameProperty)->second);

  EXPECT_EQ("internet3.voicestream.com",
            operator_.apn_list()[3].find(kApnProperty)->second);
  EXPECT_EQ(operator_.apn_list()[3].end(),
            operator_.apn_list()[3].find(kApnUsernameProperty));
  EXPECT_EQ(operator_.apn_list()[3].end(),
            operator_.apn_list()[3].find(kApnPasswordProperty));
  EXPECT_EQ(operator_.apn_list()[3].end(),
            operator_.apn_list()[3].find(kApnLocalizedNameProperty));
  EXPECT_EQ(operator_.apn_list()[3].end(),
            operator_.apn_list()[3].find(kApnLanguageProperty));
  EXPECT_EQ("Internet with VPN (old)",
            operator_.apn_list()[3].find(kApnNameProperty)->second);
  Mock::VerifyAndClearExpectations(&observer_);

  // Set from SIM. This shouldn't update the APN list.
  EXPECT_CALL(observer_, OnApnListChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnServingOperatorInfoChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnOnlinePaymentUrlTemplateChanged(_)).Times(0);
  EXPECT_CALL(observer_, OnHomeProviderInfoChanged(_)).Times(1);
  operator_.SimOperatorInfoReceived(kTestOperatorInfoCodeMccmnc, "");
  EXPECT_FALSE(operator_.serving_operator().empty());
  EXPECT_TRUE(operator_.online_payment_url_template().empty());
  EXPECT_FALSE(operator_.home_provider().empty());
  EXPECT_EQ(4, operator_.apn_list().size());
}

}  // namespace shill

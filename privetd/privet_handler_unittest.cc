// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/privet_handler.h"

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/run_loop.h>
#include <base/strings/string_util.h>
#include <base/values.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

using testing::_;
using testing::Return;

namespace privetd {

namespace {

void LoadTestJson(const std::string& test_json,
                  base::DictionaryValue* dictionary) {
  std::string json = test_json;
  base::ReplaceChars(json, "'", "\"", &json);
  int error = 0;
  std::string message;
  std::unique_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
      json, base::JSON_PARSE_RFC, &error, &message));
  EXPECT_TRUE(value.get()) << "\nError: " << message << "\n" << json;
  base::DictionaryValue* dictionary_ptr = nullptr;
  if (value->GetAsDictionary(&dictionary_ptr))
    dictionary->MergeDictionary(dictionary_ptr);
}

bool IsEqualJson(const std::string& test_json,
                 const base::DictionaryValue& dictionary) {
  base::DictionaryValue dictionary2;
  LoadTestJson(test_json, &dictionary2);
  return dictionary2.Equals(&dictionary);
}

}  // namespace

class MockPrivetHandlerDelegate : public DeviceDelegate,
                                  public WifiDelegate,
                                  public CloudDelegate,
                                  public SecurityDelegate {
  using IntPair = std::pair<int, int>;

 public:
  // Device
  MOCK_CONST_METHOD0(GetId, std::string());
  MOCK_CONST_METHOD0(GetName, std::string());
  MOCK_CONST_METHOD0(GetDescription, std::string());
  MOCK_CONST_METHOD0(GetLocation, std::string());
  MOCK_CONST_METHOD0(GetTypes, std::vector<std::string>());
  MOCK_CONST_METHOD0(GetHttpEnpoint, IntPair());
  MOCK_CONST_METHOD0(GetHttpsEnpoint, IntPair());
  MOCK_CONST_METHOD0(GetUptime, base::TimeDelta());
  MOCK_METHOD1(SetName, void(const std::string&));
  MOCK_METHOD1(SetDescription, void(const std::string&));
  MOCK_METHOD1(SetLocation, void(const std::string&));

  // Wifi
  MOCK_CONST_METHOD0(GetWifiSsid, std::string());
  MOCK_CONST_METHOD0(GetWifiTypes, std::vector<WifiType>());
  MOCK_CONST_METHOD0(IsWifiRequired, bool());
  MOCK_CONST_METHOD0(GetWifiSetupState, WifiSetupState());
  MOCK_METHOD2(SetupWifi, bool(const std::string&, const std::string&));

  // Cloud
  MOCK_CONST_METHOD0(IsRegistrationRequired, bool());
  MOCK_CONST_METHOD0(GetCloudId, std::string());
  MOCK_CONST_METHOD0(GetConnectionState, CloudState());
  MOCK_CONST_METHOD0(GetRegistrationState, RegistrationState());
  MOCK_METHOD2(RegisterDevice, bool(const std::string&, const std::string&));

  // Security
  MOCK_CONST_METHOD1(CreateAccessToken, std::string(AuthScope));
  MOCK_CONST_METHOD2(GetScopeFromAccessToken,
                     AuthScope(const std::string&, const base::Time&));
  MOCK_CONST_METHOD0(GetPairingTypes, std::vector<PairingType>());
  MOCK_CONST_METHOD1(IsValidPairingCode, bool(const std::string&));
};

class PrivetHandlerTest : public testing::Test {
 public:
  PrivetHandlerTest()
      : handler_(&delegate_, &delegate_, &delegate_, &delegate_) {}

 protected:
  void SetUp() override { auth_header_ = "Privet anonymous"; }

  const base::DictionaryValue& HandleRequest(const std::string& api,
                                             const std::string& json_input) {
    output_.Clear();
    base::DictionaryValue dictionary;
    LoadTestJson(json_input, &dictionary);
    EXPECT_TRUE(
        handler_.HandleRequest(api,
                               auth_header_,
                               dictionary,
                               base::Bind(&PrivetHandlerTest::HandlerCallback,
                                          base::Unretained(this))));
    base::RunLoop().RunUntilIdle();
    return output_;
  }

  bool HandleUnknownRequest(const std::string& api) {
    output_.Clear();
    base::DictionaryValue dictionary;
    return handler_.HandleRequest(
        api, auth_header_, dictionary, PrivetHandler::RequestCallback());
  }

  testing::StrictMock<MockPrivetHandlerDelegate> delegate_;
  std::string auth_header_;

 private:
  void HandlerCallback(const base::DictionaryValue& output, bool success) {
    EXPECT_NE(output.HasKey("reason"), success);
    output_.MergeDictionary(&output);
  }

  base::MessageLoop message_loop_;
  PrivetHandler handler_;
  base::DictionaryValue output_;
};

TEST_F(PrivetHandlerTest, UnknownApi) {
  EXPECT_FALSE(HandleUnknownRequest("/privet/foo"));
}

TEST_F(PrivetHandlerTest, MissingAuth) {
  auth_header_ = "";
  EXPECT_PRED2(IsEqualJson,
               "{'reason': 'missingAuthorization'}",
               HandleRequest("/privet/info", "{}"));
}

TEST_F(PrivetHandlerTest, InvalidAuth) {
  auth_header_ = "foo";
  EXPECT_PRED2(IsEqualJson,
               "{'reason': 'invalidAuthorization'}",
               HandleRequest("/privet/info", "{}"));
}

TEST_F(PrivetHandlerTest, InvalidAuthScope) {
  EXPECT_PRED2(IsEqualJson,
               "{'reason': 'invalidAuthorizationScope'}",
               HandleRequest("/privet/v3/setup/start", "{}"));
}

TEST_F(PrivetHandlerTest, Info) {
  EXPECT_CALL(delegate_, GetId()).WillRepeatedly(Return("TestId"));
  EXPECT_CALL(delegate_, GetName()).WillRepeatedly(Return("TestDevice"));
  EXPECT_CALL(delegate_, GetDescription())
      .WillRepeatedly(Return("TestDescription"));
  EXPECT_CALL(delegate_, GetLocation()).WillRepeatedly(Return("TestLocation"));
  EXPECT_CALL(delegate_, GetTypes())
      .WillRepeatedly(Return(std::vector<std::string>(1, "TestType")));
  EXPECT_CALL(delegate_, GetCloudId()).WillRepeatedly(Return("TestCloudId"));
  EXPECT_CALL(delegate_, GetConnectionState())
      .WillRepeatedly(Return(CloudState::kOnline));
  EXPECT_CALL(delegate_, GetHttpEnpoint())
      .WillRepeatedly(Return(std::make_pair(80, 10080)));
  EXPECT_CALL(delegate_, GetHttpsEnpoint())
      .WillRepeatedly(Return(std::make_pair(443, 10443)));
  EXPECT_CALL(delegate_, GetWifiSsid()).WillRepeatedly(Return("TestSsid"));
  EXPECT_CALL(delegate_, GetUptime())
      .WillRepeatedly(Return(base::TimeDelta::FromHours(1)));
  EXPECT_CALL(delegate_, GetWifiTypes())
      .WillRepeatedly(Return(std::vector<WifiType>(1, WifiType::kWifi24)));
  EXPECT_CALL(delegate_, GetPairingTypes())
      .WillRepeatedly(
          Return(std::vector<PairingType>(1, PairingType::kEmbeddedCode)));

  const char kExpected[] = R"({
    'version': '3.0',
    'id': 'TestId',
    'name': 'TestDevice',
    'description': 'TestDescription',
    'location': 'TestLocation',
    'type': [
      'TestType'
    ],
    'authentication': [
      'anonymous',
      'pairing',
      'cloud'
    ],
    'cloudId': 'TestCloudId',
    'cloudConnection': 'online',
    'capabilities': {
      'wireless': [
        'wifi2.4'
      ],
      'pairing': [
        'embeddedCode'
      ]
    },
    'endpoints': {
      'httpPort': 80,
      'httpUpdatesPort': 10080,
      'httpsPort': 443,
      'httpsUpdatesPort': 10443
    },
    'wifiSSID':'TestSsid',
    'uptime': 3600,
    'api': [
      '/privet/info',
      '/privet/v3/auth',
      '/privet/v3/setup/start',
      '/privet/v3/setup/status'
    ]
  })";
  EXPECT_PRED2(IsEqualJson, kExpected, HandleRequest("/privet/info", "{}"));
}

TEST_F(PrivetHandlerTest, AuthErrorNoType) {
  EXPECT_PRED2(IsEqualJson,
               "{'reason': 'invalidAuthCodeType'}",
               HandleRequest("/privet/v3/auth", "{}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidType) {
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidAuthCodeType'}",
               HandleRequest("/privet/v3/auth", "{'authCodeType':'unknown'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorNoScope) {
  EXPECT_PRED2(
      IsEqualJson,
      "{'reason':'invalidRequestedScope'}",
      HandleRequest("/privet/v3/auth", "{'authCodeType':'anonymous'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidScope) {
  EXPECT_PRED2(
      IsEqualJson,
      "{'reason':'invalidRequestedScope'}",
      HandleRequest("/privet/v3/auth",
                    "{'authCodeType':'anonymous','requestedScope':'unknown'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorAccessDenied) {
  // TODO(vitalybuka): Should fail when pairing is implemented.
  EXPECT_CALL(delegate_, CreateAccessToken(AuthScope::kOwner))
      .WillRepeatedly(Return("TestAccessToken"));
  EXPECT_PRED2(
      std::not2(std::ref(IsEqualJson)),
      "{'reason':'accessDenied'}",
      HandleRequest("/privet/v3/auth",
                    "{'authCodeType':'anonymous','requestedScope':'owner'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidAuthCode) {
  EXPECT_CALL(delegate_, IsValidPairingCode("testToken"))
      .WillRepeatedly(Return(false));
  const char kInput[] = R"({
    'authCodeType': 'pairing',
    'requestedScope': 'user',
    'authCode': 'testToken'
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidAuthCode'}",
               HandleRequest("/privet/v3/auth", kInput));
}

TEST_F(PrivetHandlerTest, AuthAnonymous) {
  EXPECT_CALL(delegate_, CreateAccessToken(AuthScope::kOwner))
      .WillRepeatedly(Return("TestAccessToken"));

  // TODO(vitalybuka): Should have anonymous scope when pairing is implemented.
  const char kExpected[] = R"({
    'accessToken': 'TestAccessToken',
    'expiresIn': 3600,
    'scope': 'owner',
    'tokenType': 'Privet'
  })";
  EXPECT_PRED2(
      IsEqualJson,
      kExpected,
      HandleRequest("/privet/v3/auth",
                    "{'authCodeType':'anonymous','requestedScope':'auto'}"));
}

TEST_F(PrivetHandlerTest, AuthPairing) {
  EXPECT_CALL(delegate_, CreateAccessToken(AuthScope::kOwner))
      .WillRepeatedly(Return("TestAccessToken"));
  EXPECT_CALL(delegate_, IsValidPairingCode("testToken"))
      .WillRepeatedly(Return(true));

  const char kInput[] = R"({
    'authCodeType': 'pairing',
    'requestedScope': 'owner',
    'authCode': 'testToken'
  })";
  const char kExpected[] = R"({
    'accessToken': 'TestAccessToken',
    'expiresIn': 3600,
    'scope': 'owner',
    'tokenType': 'Privet'
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/auth", kInput));
}

class PrivetHandlerSetupTest : public PrivetHandlerTest {
 public:
  void SetUp() override {
    PrivetHandlerTest::SetUp();
    auth_header_ = "Privet 123";
    EXPECT_CALL(delegate_, GetScopeFromAccessToken(_, _))
        .WillRepeatedly(Return(AuthScope::kOwner));
    EXPECT_CALL(delegate_, GetConnectionState())
        .WillRepeatedly(Return(CloudState::kDisabled));
    EXPECT_CALL(delegate_, GetWifiTypes())
        .WillRepeatedly(Return(std::vector<WifiType>()));
  }

  void SetupWifiStatus() {
    EXPECT_CALL(delegate_, GetWifiTypes())
        .WillRepeatedly(Return(std::vector<WifiType>(1, WifiType::kWifi24)));
    EXPECT_CALL(delegate_, IsWifiRequired()).WillRepeatedly(Return(true));
    EXPECT_CALL(delegate_, GetWifiSetupState())
        .WillRepeatedly(Return(WifiSetupState::kCompleted));
    EXPECT_CALL(delegate_, GetWifiSsid()).WillRepeatedly(Return("TestSsid"));
  }

  void SetupRegistrationStatus() {
    EXPECT_CALL(delegate_, GetConnectionState())
        .WillRepeatedly(Return(CloudState::kOnline));
    EXPECT_CALL(delegate_, IsRegistrationRequired())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(delegate_, GetRegistrationState())
        .WillRepeatedly(Return(RegistrationState::kCompleted));
    EXPECT_CALL(delegate_, GetCloudId()).WillRepeatedly(Return("TestCloudId"));
  }
};

TEST_F(PrivetHandlerSetupTest, StatusEmpty) {
  EXPECT_PRED2(
      IsEqualJson, "{}", HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusWifi) {
  SetupWifiStatus();

  const char kExpected[] = R"({
    'wifi': {
        'required': true,
        'ssid': 'TestSsid',
        'status': 'complete'
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusWifiError) {
  SetupWifiStatus();
  EXPECT_CALL(delegate_, GetWifiSetupState())
      .WillRepeatedly(Return(WifiSetupState::kInvalidPassword));

  const char kExpected[] = R"({
    'wifi': {
        'required': true,
        'status': 'error',
        'error': {
          'reason': 'invalidPassphrase'
        }
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusRegistration) {
  SetupRegistrationStatus();

  const char kExpected[] = R"({
    'registration': {
        'required': false,
        'id': 'TestCloudId',
        'status': 'complete'
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusRegistrationError) {
  SetupRegistrationStatus();
  EXPECT_CALL(delegate_, GetRegistrationState())
      .WillRepeatedly(Return(RegistrationState::kInvalidTicket));

  const char kExpected[] = R"({
    'registration': {
        'required': false,
        'status': 'error',
        'error': {
          'reason': 'invalidTicket'
        }
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StartNameDescriptionLocation) {
  EXPECT_CALL(delegate_, SetName("testName")).Times(1);
  EXPECT_CALL(delegate_, SetDescription("testDescription")).Times(1);
  EXPECT_CALL(delegate_, SetLocation("testLocation")).Times(1);
  const char kInput[] = R"({
    'name': 'testName',
    'description': 'testDescription',
    'location': 'testLocation'
  })";
  SetupWifiStatus();
  SetupRegistrationStatus();
  EXPECT_FALSE(HandleRequest("/privet/v3/setup/start", kInput).empty());
}

TEST_F(PrivetHandlerSetupTest, InvalidParams) {
  EXPECT_CALL(delegate_, GetRegistrationState())
      .WillRepeatedly(Return(RegistrationState::kAvalible));
  EXPECT_CALL(delegate_, GetWifiTypes())
      .WillRepeatedly(Return(std::vector<WifiType>(1, WifiType::kWifi50)));

  const char kInputWifi[] = R"({
    'wifi': {
      'ssid': ''
    }
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidParams'}",
               HandleRequest("/privet/v3/setup/start", kInputWifi));

  const char kInputRegistration[] = R"({
    'registration': {
      'ticketID': ''
    }
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidParams'}",
               HandleRequest("/privet/v3/setup/start", kInputRegistration));
}

TEST_F(PrivetHandlerSetupTest, WifiSetup) {
  const char kInput[] = R"({
    'wifi': {
      'ssid': 'testSsid',
      'passphrase': 'testPass'
    }
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'setupUnavailable'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  SetupWifiStatus();
  EXPECT_CALL(delegate_, SetupWifi(_, _)).WillOnce(Return(false));
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'deviceBusy'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  EXPECT_CALL(delegate_, SetupWifi("testSsid", "testPass"))
      .WillOnce(Return(true));
  EXPECT_FALSE(HandleRequest("/privet/v3/setup/start", kInput).empty());
}

TEST_F(PrivetHandlerSetupTest, Registration) {
  const char kInput[] = R"({
    'registration': {
      'ticketID': 'testTicket',
      'user': 'testUser'
    }
  })";

  EXPECT_CALL(delegate_, GetRegistrationState())
      .WillRepeatedly(Return(RegistrationState::kCompleted));
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'setupUnavailable'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  SetupRegistrationStatus();
  EXPECT_CALL(delegate_, GetRegistrationState())
      .WillRepeatedly(Return(RegistrationState::kAvalible));
  EXPECT_CALL(delegate_, RegisterDevice(_, _)).WillOnce(Return(false));
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'deviceBusy'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  EXPECT_CALL(delegate_, RegisterDevice("testTicket", "testUser"))
      .WillOnce(Return(true));
  EXPECT_FALSE(HandleRequest("/privet/v3/setup/start", kInput).empty());
}

}  // namespace privetd

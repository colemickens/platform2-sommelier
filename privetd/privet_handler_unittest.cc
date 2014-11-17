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
using testing::SetArgPointee;

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

bool IsEqualValue(const base::Value& val1, const base::Value& val2) {
  return val1.Equals(&val2);
}

bool IsEqualJson(const std::string& test_json,
                 const base::DictionaryValue& dictionary) {
  base::DictionaryValue dictionary2;
  LoadTestJson(test_json, &dictionary2);

  base::DictionaryValue::Iterator it1(dictionary);
  base::DictionaryValue::Iterator it2(dictionary2);
  for (; !it1.IsAtEnd() && !it2.IsAtEnd(); it1.Advance(), it2.Advance()) {
    // Output mismatched keys.
    EXPECT_EQ(it2.key(), it1.key());
    // Output mismatched values.
    EXPECT_PRED2(IsEqualValue, it2.value(), it1.value());
    if (it1.key() != it2.key() || !it1.value().Equals(&it2.value()))
      return false;
  }

  return it1.IsAtEnd() && it2.IsAtEnd();
}

bool IsNotEqualJson(const std::string& test_json,
                    const base::DictionaryValue& dictionary) {
  base::DictionaryValue dictionary2;
  LoadTestJson(test_json, &dictionary2);
  return !dictionary.Equals(&dictionary2);
}

}  // namespace

class MockDeviceDelegate : public DeviceDelegate {
  using IntPair = std::pair<uint16_t, uint16_t>;

 public:
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
  MOCK_METHOD1(AddType, void(const std::string&));
  MOCK_METHOD1(RemoveType, void(const std::string&));

  MockDeviceDelegate() {
    EXPECT_CALL(*this, GetId()).WillRepeatedly(Return("TestId"));
    EXPECT_CALL(*this, GetName()).WillRepeatedly(Return("TestDevice"));
    EXPECT_CALL(*this, GetDescription()).WillRepeatedly(Return(""));
    EXPECT_CALL(*this, GetLocation()).WillRepeatedly(Return(""));
    EXPECT_CALL(*this, GetTypes())
        .WillRepeatedly(Return(std::vector<std::string>{}));
    EXPECT_CALL(*this, GetHttpEnpoint())
        .WillRepeatedly(Return(std::make_pair(0, 0)));
    EXPECT_CALL(*this, GetHttpsEnpoint())
        .WillRepeatedly(Return(std::make_pair(0, 0)));
    EXPECT_CALL(*this, GetUptime())
        .WillRepeatedly(Return(base::TimeDelta::FromHours(1)));
  }
};

class MockSecurityDelegate : public SecurityDelegate {
 public:
  MOCK_CONST_METHOD2(CreateAccessToken,
                     std::string(AuthScope, const base::Time&));
  MOCK_CONST_METHOD2(ParseAccessToken,
                     AuthScope(const std::string&, base::Time*));
  MOCK_CONST_METHOD0(GetPairingTypes, std::vector<PairingType>());
  MOCK_CONST_METHOD1(IsValidPairingCode, bool(const std::string&));
  MOCK_METHOD3(StartPairing, Error(PairingType, std::string*, std::string*));
  MOCK_METHOD4(ConfirmPairing,
               Error(const std::string&,
                     const std::string&,
                     std::string*,
                     std::string*));

  MockSecurityDelegate() {
    EXPECT_CALL(*this, CreateAccessToken(AuthScope::kOwner, _))
        .WillRepeatedly(Return("TestAccessToken"));

    EXPECT_CALL(*this, ParseAccessToken(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(base::Time::Now()),
                              Return(AuthScope::kOwner)));

    EXPECT_CALL(*this, GetPairingTypes())
        .WillRepeatedly(Return(std::vector<PairingType>{
            PairingType::kPinCode,
            PairingType::kEmbeddedCode,
            PairingType::kUltrasoundDsssBroadcaster,
            PairingType::kAudibleDtmfBroadcaster,
        }));

    EXPECT_CALL(*this, StartPairing(_, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>("testSession"),
                              SetArgPointee<2>("testCommitment"),
                              Return(Error::kNone)));

    EXPECT_CALL(*this, ConfirmPairing(_, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<2>("testFingerprint"),
                              SetArgPointee<3>("testSignature"),
                              Return(Error::kNone)));
  }
};

class MockWifiDelegate : public WifiDelegate {
 public:
  MOCK_CONST_METHOD0(IsRequired, bool());
  MOCK_CONST_METHOD0(GetState, ConnectionState());
  MOCK_CONST_METHOD0(GetSetupState, SetupState());
  MOCK_METHOD2(Setup, bool(const std::string&, const std::string&));
  MOCK_CONST_METHOD0(GetSsid, std::string());
  MOCK_CONST_METHOD0(GetHostedSsid, std::string());
  MOCK_CONST_METHOD0(GetTypes, std::vector<WifiType>());

  MockWifiDelegate() {
    EXPECT_CALL(*this, IsRequired()).WillRepeatedly(Return(false));
    EXPECT_CALL(*this, GetState())
        .WillRepeatedly(Return(ConnectionState(ConnectionState::kOffline)));
    EXPECT_CALL(*this, GetSetupState())
        .WillRepeatedly(Return(SetupState(SetupState::kNone)));
    EXPECT_CALL(*this, GetSsid()).WillRepeatedly(Return("TestSsid"));
    EXPECT_CALL(*this, GetHostedSsid())
        .WillRepeatedly(Return("Test_device.BBABCLAprv"));
    EXPECT_CALL(*this, GetTypes())
        .WillRepeatedly(Return(std::vector<WifiType>{WifiType::kWifi24}));
  }
};

class MockCloudDelegate : public CloudDelegate {
 public:
  MOCK_CONST_METHOD0(IsRequired, bool());
  MOCK_CONST_METHOD0(GetState, ConnectionState());
  MOCK_CONST_METHOD0(GetSetupState, SetupState());
  MOCK_METHOD2(Setup, bool(const std::string&, const std::string&));
  MOCK_CONST_METHOD0(GetCloudId, std::string());

  MockCloudDelegate() {
    EXPECT_CALL(*this, IsRequired()).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, GetState())
        .WillRepeatedly(Return(ConnectionState(ConnectionState::kOnline)));
    EXPECT_CALL(*this, GetSetupState())
        .WillRepeatedly(Return(SetupState(SetupState::kNone)));
    EXPECT_CALL(*this, GetCloudId()).WillRepeatedly(Return("TestCloudId"));
  }
};

class PrivetHandlerTest : public testing::Test {
 public:
  PrivetHandlerTest() {}

 protected:
  void SetUp() override {
    auth_header_ = "Privet anonymous";
    handler_.reset(new PrivetHandler(&cloud_, &device_, &security_, &wifi_));
  }

  const base::DictionaryValue& HandleRequest(
      const std::string& api,
      const base::DictionaryValue* input) {
    output_.Clear();
    handler_->HandleRequest(api, auth_header_, input,
                            base::Bind(&PrivetHandlerTest::HandlerCallback,
                                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    return output_;
  }

  const base::DictionaryValue& HandleRequest(const std::string& api,
                                             const std::string& json_input) {
    base::DictionaryValue dictionary;
    LoadTestJson(json_input, &dictionary);
    return HandleRequest(api, &dictionary);
  }

  void HandleUnknownRequest(const std::string& api) {
    output_.Clear();
    base::DictionaryValue dictionary;
    handler_->HandleRequest(api, auth_header_, &dictionary,
                            base::Bind(&PrivetHandlerTest::HandlerNoFound));
    base::RunLoop().RunUntilIdle();
  }

  void SetNoWifiAndGcd() {
    handler_.reset(new PrivetHandler(nullptr, &device_, &security_, nullptr));
  }

  testing::StrictMock<MockCloudDelegate> cloud_;
  testing::StrictMock<MockDeviceDelegate> device_;
  testing::StrictMock<MockSecurityDelegate> security_;
  testing::StrictMock<MockWifiDelegate> wifi_;
  std::string auth_header_;

 private:
  void HandlerCallback(int status, const base::DictionaryValue& output) {
    EXPECT_NE(output.HasKey("reason"), status == 200);
    output_.MergeDictionary(&output);
  }

  static void HandlerNoFound(int status, const base::DictionaryValue&) {
    EXPECT_EQ(status, 404);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<PrivetHandler> handler_;
  base::DictionaryValue output_;
};

TEST_F(PrivetHandlerTest, UnknownApi) {
  HandleUnknownRequest("/privet/foo");
}

TEST_F(PrivetHandlerTest, InvalidFormat) {
  auth_header_ = "";
  EXPECT_PRED2(IsEqualJson, "{'reason': 'invalidFormat'}",
               HandleRequest("/privet/info", nullptr));
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

TEST_F(PrivetHandlerTest, InfoMinimal) {
  SetNoWifiAndGcd();
  EXPECT_CALL(security_, GetPairingTypes())
      .WillRepeatedly(Return(std::vector<PairingType>{}));

  const char kExpected[] = R"({
    'version': '3.0',
    'id': 'TestId',
    'name': 'TestDevice',
    'type': [],
    'endpoints': {
      'httpPort': 0,
      'httpUpdatesPort': 0,
      'httpsPort': 0,
      'httpsUpdatesPort': 0
    },
    'authentication': {
      'mode': [
        'anonymous',
        'pairing'
      ],
      'pairing': [
      ],
      'crypto': [
        'p256_spake2'
      ]
    },
    'uptime': 3600,
    'api': [
      '/privet/info',
      '/privet/v3/auth',
      '/privet/v3/pairing/confirm',
      '/privet/v3/pairing/start',
      '/privet/v3/setup/start',
      '/privet/v3/setup/status'
    ]
  })";
  EXPECT_PRED2(IsEqualJson, kExpected, HandleRequest("/privet/info", "{}"));
}

TEST_F(PrivetHandlerTest, Info) {
  EXPECT_CALL(device_, GetDescription())
      .WillRepeatedly(Return("TestDescription"));
  EXPECT_CALL(device_, GetLocation()).WillRepeatedly(Return("TestLocation"));
  EXPECT_CALL(device_, GetTypes())
      .WillRepeatedly(Return(std::vector<std::string>{"TestType"}));
  EXPECT_CALL(device_, GetHttpEnpoint())
      .WillRepeatedly(Return(std::make_pair(80, 10080)));
  EXPECT_CALL(device_, GetHttpsEnpoint())
      .WillRepeatedly(Return(std::make_pair(443, 10443)));

  const char kExpected[] = R"({
    'version': '3.0',
    'id': 'TestId',
    'name': 'TestDevice',
    'description': 'TestDescription',
    'location': 'TestLocation',
    'type': [
      'TestType'
    ],
    'endpoints': {
      'httpPort': 80,
      'httpUpdatesPort': 10080,
      'httpsPort': 443,
      'httpsUpdatesPort': 10443
    },
    'authentication': {
      'mode': [
        'anonymous',
        'pairing',
        'cloud'
      ],
      'pairing': [
        'pinCode',
        'embeddedCode',
        'ultrasoundDsssBroadcaster',
        'audibleDtmfBroadcaster'
      ],
      'crypto': [
        'p256_spake2'
      ]
    },
    'wifi': {
      'required': false,
      'capabilities': [
        '2.4GHz'
      ],
      'ssid': 'TestSsid',
      'hostedSsid': 'Test_device.BBABCLAprv',
      'status': 'offline'
    },
    'gcd': {
      'required': true,
      'id': 'TestCloudId',
      'status': 'online'
    },
    'uptime': 3600,
    'api': [
      '/privet/info',
      '/privet/v3/auth',
      '/privet/v3/pairing/confirm',
      '/privet/v3/pairing/start',
      '/privet/v3/setup/start',
      '/privet/v3/setup/status'
    ]
  })";
  EXPECT_PRED2(IsEqualJson, kExpected, HandleRequest("/privet/info", "{}"));
}

TEST_F(PrivetHandlerTest, PairingStartInvalidParams) {
  EXPECT_PRED2(IsEqualJson, "{'reason': 'invalidParams'}",
               HandleRequest("/privet/v3/pairing/start",
                             "{'mode': 'embeddedCode', 'crypto': 'crypto'}"));

  EXPECT_PRED2(IsEqualJson, "{'reason': 'invalidParams'}",
               HandleRequest("/privet/v3/pairing/start",
                             "{'mode': 'code', 'crypto': 'p256_spake2'}"));
}

TEST_F(PrivetHandlerTest, PairingStart) {
  EXPECT_PRED2(
      IsEqualJson,
      "{'deviceCommitment': 'testCommitment', 'sessionId': 'testSession'}",
      HandleRequest("/privet/v3/pairing/start",
                    "{'mode': 'embeddedCode', 'crypto': 'p256_spake2'}"));
}

TEST_F(PrivetHandlerTest, PairingConfirm) {
  EXPECT_PRED2(
      IsEqualJson,
      "{'certFingerprint':'testFingerprint','certSignature':'testSignature'}",
      HandleRequest(
          "/privet/v3/pairing/confirm",
          "{'sessionId':'testSession','clientCommitment':'testCommitment'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorNoType) {
  EXPECT_PRED2(IsEqualJson, "{'reason': 'invalidAuthMode'}",
               HandleRequest("/privet/v3/auth", "{}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidType) {
  EXPECT_PRED2(IsEqualJson, "{'reason':'invalidAuthMode'}",
               HandleRequest("/privet/v3/auth", "{'authMode':'unknown'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorNoScope) {
  EXPECT_PRED2(IsEqualJson, "{'reason':'invalidRequestedScope'}",
               HandleRequest("/privet/v3/auth", "{'authMode':'anonymous'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidScope) {
  EXPECT_PRED2(
      IsEqualJson, "{'reason':'invalidRequestedScope'}",
      HandleRequest("/privet/v3/auth",
                    "{'authMode':'anonymous','requestedScope':'unknown'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorAccessDenied) {
  // TODO(vitalybuka): Should fail when pairing is implemented.
  EXPECT_PRED2(
      IsNotEqualJson, "{'reason':'accessDenied'}",
      HandleRequest("/privet/v3/auth",
                    "{'authMode':'anonymous','requestedScope':'owner'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidAuthCode) {
  EXPECT_CALL(security_, IsValidPairingCode("testToken"))
      .WillRepeatedly(Return(false));
  const char kInput[] = R"({
    'authMode': 'pairing',
    'requestedScope': 'user',
    'authCode': 'testToken'
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidAuthCode'}",
               HandleRequest("/privet/v3/auth", kInput));
}

TEST_F(PrivetHandlerTest, AuthAnonymous) {
  // TODO(vitalybuka): Should have anonymous scope when pairing is implemented.
  const char kExpected[] = R"({
    'accessToken': 'TestAccessToken',
    'expiresIn': 3600,
    'scope': 'owner',
    'tokenType': 'Privet'
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected,
      HandleRequest("/privet/v3/auth",
                    "{'authMode':'anonymous','requestedScope':'auto'}"));
}

TEST_F(PrivetHandlerTest, AuthPairing) {
  EXPECT_CALL(security_, IsValidPairingCode("testToken"))
      .WillRepeatedly(Return(true));

  const char kInput[] = R"({
    'authMode': 'pairing',
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
  }
};

TEST_F(PrivetHandlerSetupTest, StatusEmpty) {
  SetNoWifiAndGcd();
  EXPECT_PRED2(
      IsEqualJson, "{}", HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusWifi) {
  EXPECT_CALL(wifi_, GetSetupState())
      .WillRepeatedly(Return(SetupState(SetupState::kSuccess)));

  const char kExpected[] = R"({
    'wifi': {
        'ssid': 'TestSsid',
        'status': 'success'
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusWifiError) {
  EXPECT_CALL(wifi_, GetSetupState())
      .WillRepeatedly(Return(SetupState(Error::kInvalidPassphrase)));

  const char kExpected[] = R"({
    'wifi': {
        'status': 'error',
        'error': {
          'reason': 'invalidPassphrase'
        }
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusGcd) {
  EXPECT_CALL(cloud_, GetSetupState())
      .WillRepeatedly(Return(SetupState(SetupState::kSuccess)));

  const char kExpected[] = R"({
    'gcd': {
        'id': 'TestCloudId',
        'status': 'success'
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, StatusGcdError) {
  EXPECT_CALL(cloud_, GetSetupState())
      .WillRepeatedly(Return(SetupState(Error::kInvalidTicket)));

  const char kExpected[] = R"({
    'gcd': {
        'status': 'error',
        'error': {
          'reason': 'invalidTicket'
        }
     }
  })";
  EXPECT_PRED2(
      IsEqualJson, kExpected, HandleRequest("/privet/v3/setup/status", "{}"));
}

TEST_F(PrivetHandlerSetupTest, SetupNameDescriptionLocation) {
  EXPECT_CALL(device_, SetName("testName")).Times(1);
  EXPECT_CALL(device_, SetDescription("testDescription")).Times(1);
  EXPECT_CALL(device_, SetLocation("testLocation")).Times(1);
  const char kInput[] = R"({
    'name': 'testName',
    'description': 'testDescription',
    'location': 'testLocation'
  })";
  EXPECT_PRED2(IsEqualJson, "{}",
               HandleRequest("/privet/v3/setup/start", kInput));
}

TEST_F(PrivetHandlerSetupTest, InvalidParams) {
  const char kInputWifi[] = R"({
    'wifi': {
      'ssid': ''
    }
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidParams'}",
               HandleRequest("/privet/v3/setup/start", kInputWifi));

  const char kInputRegistration[] = R"({
    'gcd': {
      'ticketId': ''
    }
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidParams'}",
               HandleRequest("/privet/v3/setup/start", kInputRegistration));
}

TEST_F(PrivetHandlerSetupTest, WifiSetupUnavailable) {
  SetNoWifiAndGcd();
  EXPECT_PRED2(IsEqualJson, "{'reason':'setupUnavailable'}",
               HandleRequest("/privet/v3/setup/start", "{'wifi': {}}"));
}

TEST_F(PrivetHandlerSetupTest, WifiSetup) {
  const char kInput[] = R"({
    'wifi': {
      'ssid': 'testSsid',
      'passphrase': 'testPass'
    }
  })";
  EXPECT_CALL(wifi_, Setup(_, _)).WillOnce(Return(false));
  EXPECT_PRED2(IsEqualJson, "{'reason':'deviceBusy'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  const char kExpected[] = R"({
    'wifi': {
      'status': 'inProgress'
    }
  })";
  EXPECT_CALL(wifi_, GetSetupState())
      .WillRepeatedly(Return(SetupState(SetupState::kInProgress)));
  EXPECT_CALL(wifi_, Setup("testSsid", "testPass")).WillOnce(Return(true));
  EXPECT_PRED2(IsEqualJson, kExpected,
               HandleRequest("/privet/v3/setup/start", kInput));
}

TEST_F(PrivetHandlerSetupTest, GcdSetupUnavailable) {
  SetNoWifiAndGcd();
  EXPECT_PRED2(IsEqualJson, "{'reason':'setupUnavailable'}",
               HandleRequest("/privet/v3/setup/start", "{'gcd': {}}"));
}

TEST_F(PrivetHandlerSetupTest, GcdSetup) {
  const char kInput[] = R"({
    'gcd': {
      'ticketId': 'testTicket',
      'user': 'testUser'
    }
  })";
  EXPECT_CALL(cloud_, Setup(_, _)).WillOnce(Return(false));
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'deviceBusy'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  const char kExpected[] = R"({
    'gcd': {
      'status': 'inProgress'
    }
  })";
  EXPECT_CALL(cloud_, GetSetupState())
      .WillRepeatedly(Return(SetupState(SetupState::kInProgress)));
  EXPECT_CALL(cloud_, Setup("testTicket", "testUser")).WillOnce(Return(true));
  EXPECT_PRED2(IsEqualJson, kExpected,
               HandleRequest("/privet/v3/setup/start", kInput));
}

}  // namespace privetd

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

#include "privetd/mock_delegates.h"

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

}  // namespace

class PrivetHandlerTest : public testing::Test {
 public:
  PrivetHandlerTest() {}

 protected:
  void SetUp() override {
    auth_header_ = "Privet anonymous";
    handler_.reset(
        new PrivetHandler(&cloud_, &device_, &security_, &wifi_, &identity_));
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
    handler_.reset(
        new PrivetHandler(nullptr, &device_, &security_, nullptr, &identity_));
  }

  testing::StrictMock<MockCloudDelegate> cloud_;
  testing::StrictMock<MockDeviceDelegate> device_;
  testing::StrictMock<MockSecurityDelegate> security_;
  testing::StrictMock<MockWifiDelegate> wifi_;
  testing::StrictMock<MockIdentityDelegate> identity_;
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
  EXPECT_CALL(security_, GetCryptoTypes())
      .WillRepeatedly(Return(std::vector<CryptoType>{}));

  const char kExpected[] = R"({
    'version': '3.0',
    'id': 'TestId',
    'name': 'TestDevice',
    'class': "AB",
    'modelId': "MID",
    'services': [],
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
      ]
    },
    'uptime': 3600,
    'api': [
      '/privet/info',
      '/privet/v3/auth',
      '/privet/v3/pairing/cancel',
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
  EXPECT_CALL(device_, GetServices())
      .WillRepeatedly(Return(std::vector<std::string>{"service1", "service2"}));
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
    'class': "AB",
    'modelId': "MID",
    'services': [
      "service1",
      "service2"
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
        'p224_spake2',
        'p256_spake2'
      ]
    },
    'wifi': {
      'capabilities': [
        '2.4GHz'
      ],
      'ssid': 'TestSsid',
      'hostedSsid': 'Test_device.BBABCLAprv',
      'status': 'offline'
    },
    'gcd': {
      'id': 'TestCloudId',
      'status': 'online'
    },
    'uptime': 3600,
    'api': [
      '/privet/info',
      '/privet/v3/auth',
      '/privet/v3/pairing/cancel',
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
                             "{'pairing':'embeddedCode','crypto':'crypto'}"));

  EXPECT_PRED2(IsEqualJson, "{'reason': 'invalidParams'}",
               HandleRequest("/privet/v3/pairing/start",
                             "{'pairing':'code','crypto':'p256_spake2'}"));
}

TEST_F(PrivetHandlerTest, PairingStart) {
  EXPECT_PRED2(
      IsEqualJson,
      "{'deviceCommitment': 'testCommitment', 'sessionId': 'testSession'}",
      HandleRequest("/privet/v3/pairing/start",
                    "{'pairing': 'embeddedCode', 'crypto': 'p256_spake2'}"));
}

TEST_F(PrivetHandlerTest, PairingConfirm) {
  EXPECT_PRED2(
      IsEqualJson,
      "{'certFingerprint':'testFingerprint','certSignature':'testSignature'}",
      HandleRequest(
          "/privet/v3/pairing/confirm",
          "{'sessionId':'testSession','clientCommitment':'testCommitment'}"));
}

TEST_F(PrivetHandlerTest, PairingCancel) {
  EXPECT_PRED2(IsEqualJson, "{}",
               HandleRequest("/privet/v3/pairing/cancel",
                             "{'sessionId': 'testSession'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorNoType) {
  EXPECT_PRED2(IsEqualJson, "{'reason': 'invalidAuthMode'}",
               HandleRequest("/privet/v3/auth", "{}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidType) {
  EXPECT_PRED2(IsEqualJson, "{'reason':'invalidAuthMode'}",
               HandleRequest("/privet/v3/auth", "{'mode':'unknown'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorNoScope) {
  EXPECT_PRED2(IsEqualJson, "{'reason':'invalidRequestedScope'}",
               HandleRequest("/privet/v3/auth", "{'mode':'anonymous'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidScope) {
  EXPECT_PRED2(
      IsEqualJson, "{'reason':'invalidRequestedScope'}",
      HandleRequest("/privet/v3/auth",
                    "{'mode':'anonymous','requestedScope':'unknown'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorAccessDenied) {
  EXPECT_PRED2(IsEqualJson, "{'reason':'accessDenied'}",
               HandleRequest("/privet/v3/auth",
                             "{'mode':'anonymous','requestedScope':'owner'}"));
}

TEST_F(PrivetHandlerTest, AuthErrorInvalidAuthCode) {
  EXPECT_CALL(security_, IsValidPairingCode("testToken"))
      .WillRepeatedly(Return(false));
  const char kInput[] = R"({
    'mode': 'pairing',
    'requestedScope': 'user',
    'authCode': 'testToken'
  })";
  EXPECT_PRED2(IsEqualJson,
               "{'reason':'invalidAuthCode'}",
               HandleRequest("/privet/v3/auth", kInput));
}

TEST_F(PrivetHandlerTest, AuthAnonymous) {
  const char kExpected[] = R"({
    'accessToken': 'GuestAccessToken',
    'expiresIn': 3600,
    'scope': 'guest',
    'tokenType': 'Privet'
  })";
  EXPECT_PRED2(IsEqualJson, kExpected,
               HandleRequest("/privet/v3/auth",
                             "{'mode':'anonymous','requestedScope':'auto'}"));
}

TEST_F(PrivetHandlerTest, AuthPairing) {
  EXPECT_CALL(security_, IsValidPairingCode("testToken"))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(security_, CreateAccessToken(_, _))
      .WillRepeatedly(Return("OwnerAccessToken"));
  const char kInput[] = R"({
    'mode': 'pairing',
    'requestedScope': 'owner',
    'authCode': 'testToken'
  })";
  const char kExpected[] = R"({
    'accessToken': 'OwnerAccessToken',
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
    EXPECT_CALL(security_, ParseAccessToken(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(base::Time::Now()),
                              Return(AuthScope::kOwner)));
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
  EXPECT_CALL(wifi_, ConfigureCredentials(_, _)).WillOnce(Return(false));
  EXPECT_PRED2(IsEqualJson, "{'reason':'deviceBusy'}",
               HandleRequest("/privet/v3/setup/start", kInput));

  const char kExpected[] = R"({
    'wifi': {
      'status': 'inProgress'
    }
  })";
  EXPECT_CALL(wifi_, GetSetupState())
      .WillRepeatedly(Return(SetupState(SetupState::kInProgress)));
  EXPECT_CALL(wifi_, ConfigureCredentials("testSsid", "testPass"))
      .WillOnce(Return(true));
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

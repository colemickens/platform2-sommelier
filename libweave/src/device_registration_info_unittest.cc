// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/device_registration_info.h"

#include <base/json/json_reader.h>
#include <base/json/json_writer.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/values.h>
#include <chromeos/bind_lambda.h>
#include <chromeos/key_value_store.h>
#include <chromeos/mime_utils.h>
#include <gtest/gtest.h>
#include <weave/mock_http_client.h>

#include "libweave/src/commands/command_manager.h"
#include "libweave/src/commands/unittest_utils.h"
#include "libweave/src/states/mock_state_change_queue_interface.h"
#include "libweave/src/states/state_manager.h"
#include "libweave/src/storage_impls.h"

using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using testing::StrictMock;
using testing::WithArgs;

namespace weave {

using unittests::CreateDictionaryValue;
using unittests::CreateValue;
using unittests::MockHttpClient;
using unittests::MockHttpClientResponse;

namespace {

namespace test_data {

const char kServiceURL[] = "http://gcd.server.com/";
const char kOAuthURL[] = "http://oauth.server.com/";
const char kApiKey[] = "GOadRdTf9FERf0k4w6EFOof56fUJ3kFDdFL3d7f";
const char kClientId[] =
    "123543821385-sfjkjshdkjhfk234sdfsdfkskd"
    "fkjh7f.apps.googleusercontent.com";
const char kClientSecret[] = "5sdGdGlfolGlrFKfdFlgP6FG";
const char kDeviceId[] = "4a7ea2d1-b331-1e1f-b206-e863c7635196";
const char kClaimTicketId[] = "RTcUE";
const char kAccessToken[] =
    "ya29.1.AADtN_V-dLUM-sVZ0qVjG9Dxm5NgdS9J"
    "Mx_JLUqhC9bED_YFjzHZtYt65ZzXCS35NMAeaVZ"
    "Dei530-w0yE2urpQ";
const char kRefreshToken[] =
    "1/zQmxR6PKNvhcxf9SjXUrCjcmCrcqRKXctc6cp"
    "1nI-GQ";
const char kRobotAccountAuthCode[] =
    "4/Mf_ujEhPejVhOq-OxW9F5cSOnWzx."
    "YgciVjTYGscRshQV0ieZDAqiTIjMigI";
const char kRobotAccountEmail[] =
    "6ed0b3f54f9bd619b942f4ad2441c252@"
    "clouddevices.gserviceaccount.com";

}  // namespace test_data

// Add the test device registration information.
void SetDefaultDeviceRegistration(base::DictionaryValue* data) {
  data->SetString("refresh_token", test_data::kRefreshToken);
  data->SetString("device_id", test_data::kDeviceId);
  data->SetString("robot_account", test_data::kRobotAccountEmail);
}

std::string GetFormField(const std::string& data, const std::string& name) {
  EXPECT_FALSE(data.empty());
  for (const auto& i : chromeos::data_encoding::WebParamsDecode(data)) {
    if (i.first == name)
      return i.second;
  }
  return {};
}

HttpClient::Response* ReplyWithJson(int status_code, const base::Value& json) {
  std::string text;
  base::JSONWriter::WriteWithOptions(
      json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &text);

  MockHttpClientResponse* response = new StrictMock<MockHttpClientResponse>;
  EXPECT_CALL(*response, GetStatusCode())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(status_code));
  EXPECT_CALL(*response, GetContentType())
      .Times(AtLeast(1))
      .WillRepeatedly(Return("application/json"));
  EXPECT_CALL(*response, GetData())
      .Times(AtLeast(1))
      .WillRepeatedly(ReturnRefOfCopy(text));
  return response;
}

std::pair<std::string, std::string> GetAuthHeader() {
  return {std::string{"Authorization"},
          std::string{"Bearer "} + test_data::kAccessToken};
}

}  // anonymous namespace

class DeviceRegistrationInfoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(mock_state_change_queue_, GetLastStateChangeId())
        .WillRepeatedly(Return(0));
    EXPECT_CALL(mock_state_change_queue_, MockAddOnStateUpdatedCallback(_))
        .WillRepeatedly(Return(nullptr));

    std::unique_ptr<StorageInterface> storage{new MemStorage};
    storage_ = storage.get();
    storage->Save(data_);
    command_manager_ = std::make_shared<CommandManager>();
    state_manager_ = std::make_shared<StateManager>(&mock_state_change_queue_);

    std::unique_ptr<BuffetConfig> config{new BuffetConfig{std::move(storage)}};
    config_ = config.get();
    dev_reg_.reset(new DeviceRegistrationInfo{command_manager_, state_manager_,
                                              std::move(config), &http_client_,
                                              nullptr, true, nullptr});

    ReloadConfig();
  }

  void ReloadConfig() {
    chromeos::KeyValueStore config_store;
    config_store.SetString("client_id", test_data::kClientId);
    config_store.SetString("client_secret", test_data::kClientSecret);
    config_store.SetString("api_key", test_data::kApiKey);
    config_store.SetString("device_kind", "vendor");
    config_store.SetString("name", "Coffee Pot");
    config_store.SetString("description", "Easy to clean");
    config_store.SetString("location", "Kitchen");
    config_store.SetString("local_anonymous_access_role", "viewer");
    config_store.SetString("model_id", "AAAAA");
    config_store.SetString("oauth_url", test_data::kOAuthURL);
    config_store.SetString("service_url", test_data::kServiceURL);
    config_->Load(config_store);
    dev_reg_->Start();
  }

  void PublishCommands(const base::ListValue& commands) {
    return dev_reg_->PublishCommands(commands);
  }

  bool RefreshAccessToken(chromeos::ErrorPtr* error) const {
    base::MessageLoopForIO message_loop;
    base::RunLoop run_loop;

    bool succeeded = false;
    auto on_success = [&run_loop, &succeeded]() {
      succeeded = true;
      run_loop.Quit();
    };
    auto on_failure = [&run_loop, &error](const chromeos::Error* in_error) {
      if (error)
        *error = in_error->Clone();
      run_loop.Quit();
    };
    dev_reg_->RefreshAccessToken(base::Bind(on_success),
                                 base::Bind(on_failure));
    run_loop.Run();
    return succeeded;
  }

  void SetAccessToken() { dev_reg_->access_token_ = test_data::kAccessToken; }

  RegistrationStatus GetRegistrationStatus() const {
    return dev_reg_->registration_status_;
  }

  StrictMock<MockHttpClient> http_client_;
  base::DictionaryValue data_;
  StorageInterface* storage_{nullptr};
  BuffetConfig* config_{nullptr};
  std::unique_ptr<DeviceRegistrationInfo> dev_reg_;
  std::shared_ptr<CommandManager> command_manager_;
  StrictMock<MockStateChangeQueueInterface> mock_state_change_queue_;
  std::shared_ptr<StateManager> state_manager_;
};

////////////////////////////////////////////////////////////////////////////////
TEST_F(DeviceRegistrationInfoTest, GetServiceURL) {
  EXPECT_EQ(test_data::kServiceURL, dev_reg_->GetServiceURL());
  std::string url = test_data::kServiceURL;
  url += "registrationTickets";
  EXPECT_EQ(url, dev_reg_->GetServiceURL("registrationTickets"));
  url += "?key=";
  url += test_data::kApiKey;
  EXPECT_EQ(url, dev_reg_->GetServiceURL("registrationTickets",
                                         {{"key", test_data::kApiKey}}));
  url += "&restart=true";
  EXPECT_EQ(url, dev_reg_->GetServiceURL(
                     "registrationTickets",
                     {
                         {"key", test_data::kApiKey}, {"restart", "true"},
                     }));
}

TEST_F(DeviceRegistrationInfoTest, GetOAuthURL) {
  EXPECT_EQ(test_data::kOAuthURL, dev_reg_->GetOAuthURL());
  std::string url = test_data::kOAuthURL;
  url += "auth?scope=https%3A%2F%2Fwww.googleapis.com%2Fauth%2Fclouddevices&";
  url += "redirect_uri=urn%3Aietf%3Awg%3Aoauth%3A2.0%3Aoob&";
  url += "response_type=code&";
  url += "client_id=";
  url += test_data::kClientId;
  EXPECT_EQ(url, dev_reg_->GetOAuthURL(
                     "auth",
                     {{"scope", "https://www.googleapis.com/auth/clouddevices"},
                      {"redirect_uri", "urn:ietf:wg:oauth:2.0:oob"},
                      {"response_type", "code"},
                      {"client_id", test_data::kClientId}}));
}

TEST_F(DeviceRegistrationInfoTest, HaveRegistrationCredentials) {
  EXPECT_FALSE(dev_reg_->HaveRegistrationCredentials());
  SetDefaultDeviceRegistration(&data_);
  storage_->Save(data_);
  ReloadConfig();

  EXPECT_CALL(http_client_,
              MockSendRequest("POST", dev_reg_->GetOAuthURL("token"), _,
                              "application/x-www-form-urlencoded",
                              HttpClient::Headers{}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_EQ("refresh_token", GetFormField(data, "grant_type"));
        EXPECT_EQ(test_data::kRefreshToken,
                  GetFormField(data, "refresh_token"));
        EXPECT_EQ(test_data::kClientId, GetFormField(data, "client_id"));
        EXPECT_EQ(test_data::kClientSecret,
                  GetFormField(data, "client_secret"));

        base::DictionaryValue json;
        json.SetString("access_token", test_data::kAccessToken);
        json.SetInteger("expires_in", 3600);

        return ReplyWithJson(200, json);
      })));

  EXPECT_TRUE(RefreshAccessToken(nullptr));
  EXPECT_TRUE(dev_reg_->HaveRegistrationCredentials());
}

TEST_F(DeviceRegistrationInfoTest, CheckAuthenticationFailure) {
  SetDefaultDeviceRegistration(&data_);
  storage_->Save(data_);
  ReloadConfig();
  EXPECT_EQ(RegistrationStatus::kConnecting, GetRegistrationStatus());

  EXPECT_CALL(http_client_,
              MockSendRequest("POST", dev_reg_->GetOAuthURL("token"), _,
                              "application/x-www-form-urlencoded",
                              HttpClient::Headers{}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_EQ("refresh_token", GetFormField(data, "grant_type"));
        EXPECT_EQ(test_data::kRefreshToken,
                  GetFormField(data, "refresh_token"));
        EXPECT_EQ(test_data::kClientId, GetFormField(data, "client_id"));
        EXPECT_EQ(test_data::kClientSecret,
                  GetFormField(data, "client_secret"));

        base::DictionaryValue json;
        json.SetString("error", "unable_to_authenticate");
        return ReplyWithJson(400, json);
      })));

  chromeos::ErrorPtr error;
  EXPECT_FALSE(RefreshAccessToken(&error));
  EXPECT_TRUE(error->HasError(kErrorDomainOAuth2, "unable_to_authenticate"));
  EXPECT_EQ(RegistrationStatus::kConnecting, GetRegistrationStatus());
}

TEST_F(DeviceRegistrationInfoTest, CheckDeregistration) {
  SetDefaultDeviceRegistration(&data_);
  storage_->Save(data_);
  ReloadConfig();
  EXPECT_EQ(RegistrationStatus::kConnecting, GetRegistrationStatus());

  EXPECT_CALL(http_client_,
              MockSendRequest("POST", dev_reg_->GetOAuthURL("token"), _,
                              "application/x-www-form-urlencoded",
                              HttpClient::Headers{}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_EQ("refresh_token", GetFormField(data, "grant_type"));
        EXPECT_EQ(test_data::kRefreshToken,
                  GetFormField(data, "refresh_token"));
        EXPECT_EQ(test_data::kClientId, GetFormField(data, "client_id"));
        EXPECT_EQ(test_data::kClientSecret,
                  GetFormField(data, "client_secret"));

        base::DictionaryValue json;
        json.SetString("error", "invalid_grant");
        return ReplyWithJson(400, json);
      })));

  chromeos::ErrorPtr error;
  EXPECT_FALSE(RefreshAccessToken(&error));
  EXPECT_TRUE(error->HasError(kErrorDomainOAuth2, "invalid_grant"));
  EXPECT_EQ(RegistrationStatus::kInvalidCredentials, GetRegistrationStatus());
}

TEST_F(DeviceRegistrationInfoTest, GetDeviceInfo) {
  SetDefaultDeviceRegistration(&data_);
  storage_->Save(data_);
  ReloadConfig();
  SetAccessToken();

  EXPECT_CALL(http_client_,
              MockSendRequest("GET", dev_reg_->GetDeviceURL(), _,
                              "application/json; charset=utf-8",
                              HttpClient::Headers{GetAuthHeader()}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        base::DictionaryValue json;
        json.SetString("channel.supportedType", "xmpp");
        json.SetString("deviceKind", "vendor");
        json.SetString("id", test_data::kDeviceId);
        json.SetString("kind", "clouddevices#device");
        return ReplyWithJson(200, json);
      })));

  base::MessageLoopForIO message_loop;
  base::RunLoop run_loop;

  bool succeeded = false;
  auto on_success = [&run_loop, &succeeded,
                     this](const base::DictionaryValue& info) {
    std::string id;
    EXPECT_TRUE(info.GetString("id", &id));
    EXPECT_EQ(test_data::kDeviceId, id);
    succeeded = true;
    run_loop.Quit();
  };
  auto on_failure = [&run_loop](const chromeos::Error* error) {
    run_loop.Quit();
    FAIL() << "Should not be called";
  };
  dev_reg_->GetDeviceInfo(base::Bind(on_success), base::Bind(on_failure));
  run_loop.Run();
  EXPECT_TRUE(succeeded);
}

TEST_F(DeviceRegistrationInfoTest, RegisterDevice) {
  auto json_base = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': 'integer'},
        'minimalRole': 'user',
        'results': {}
      },
      'shutdown': {
        'parameters': {},
        'minimalRole': 'user',
        'results': {}
      }
    }
  })");
  EXPECT_TRUE(command_manager_->LoadBaseCommands(*json_base, nullptr));
  auto json_cmds = CreateDictionaryValue(R"({
    'base': {
      'reboot': {
        'parameters': {'delay': {'minimum': 10}},
        'minimalRole': 'user',
        'results': {}
      }
    },
    'robot': {
      '_jump': {
        'parameters': {'_height': 'integer'},
        'minimalRole': 'user',
        'results': {}
      }
    }
  })");
  EXPECT_TRUE(command_manager_->LoadCommands(*json_cmds, "", nullptr));

  std::string ticket_url = dev_reg_->GetServiceURL("registrationTickets/") +
                           test_data::kClaimTicketId;
  EXPECT_CALL(http_client_,
              MockSendRequest(
                  "PATCH", ticket_url + "?key=" + test_data::kApiKey, _,
                  "application/json; charset=utf-8", HttpClient::Headers{}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        auto json = unittests::CreateDictionaryValue(data);
        EXPECT_NE(nullptr, json.get());
        std::string value;
        EXPECT_TRUE(json->GetString("id", &value));
        EXPECT_EQ(test_data::kClaimTicketId, value);
        EXPECT_TRUE(
            json->GetString("deviceDraft.channel.supportedType", &value));
        EXPECT_EQ("pull", value);
        EXPECT_TRUE(json->GetString("oauthClientId", &value));
        EXPECT_EQ(test_data::kClientId, value);
        EXPECT_TRUE(json->GetString("deviceDraft.deviceKind", &value));
        EXPECT_EQ("vendor", value);
        EXPECT_TRUE(json->GetString("deviceDraft.description", &value));
        EXPECT_EQ("Easy to clean", value);
        EXPECT_TRUE(json->GetString("deviceDraft.location", &value));
        EXPECT_EQ("Kitchen", value);
        EXPECT_TRUE(json->GetString("deviceDraft.modelManifestId", &value));
        EXPECT_EQ("AAAAA", value);
        EXPECT_TRUE(json->GetString("deviceDraft.name", &value));
        EXPECT_EQ("Coffee Pot", value);
        base::DictionaryValue* commandDefs = nullptr;
        EXPECT_TRUE(
            json->GetDictionary("deviceDraft.commandDefs", &commandDefs));
        EXPECT_FALSE(commandDefs->empty());

        auto expected = R"({
            'base': {
              'reboot': {
                'parameters': {
                  'delay': {
                    'minimum': 10,
                    'type': 'integer'
                  }
                },
                'minimalRole': 'user'
              }
            },
            'robot': {
              '_jump': {
                'parameters': {
                  '_height': {
                    'type': 'integer'
                  }
                },
                'minimalRole': 'user'
              }
            }
          })";
        EXPECT_JSON_EQ(expected, *commandDefs);

        base::DictionaryValue json_resp;
        json_resp.SetString("id", test_data::kClaimTicketId);
        json_resp.SetString("kind", "clouddevices#registrationTicket");
        json_resp.SetString("oauthClientId", test_data::kClientId);
        base::DictionaryValue* device_draft = nullptr;
        EXPECT_TRUE(json->GetDictionary("deviceDraft", &device_draft));
        device_draft = device_draft->DeepCopy();
        device_draft->SetString("id", test_data::kDeviceId);
        device_draft->SetString("kind", "clouddevices#device");
        json_resp.Set("deviceDraft", device_draft);

        return ReplyWithJson(200, json_resp);
      })));

  EXPECT_CALL(http_client_,
              MockSendRequest(
                  "POST", ticket_url + "/finalize?key=" + test_data::kApiKey,
                  "", _, HttpClient::Headers{}, _))
      .WillOnce(InvokeWithoutArgs([]() {
        base::DictionaryValue json;
        json.SetString("id", test_data::kClaimTicketId);
        json.SetString("kind", "clouddevices#registrationTicket");
        json.SetString("oauthClientId", test_data::kClientId);
        json.SetString("userEmail", "user@email.com");
        json.SetString("deviceDraft.id", test_data::kDeviceId);
        json.SetString("deviceDraft.kind", "clouddevices#device");
        json.SetString("deviceDraft.channel.supportedType", "xmpp");
        json.SetString("robotAccountEmail", test_data::kRobotAccountEmail);
        json.SetString("robotAccountAuthorizationCode",
                       test_data::kRobotAccountAuthCode);
        return ReplyWithJson(200, json);
      }));

  EXPECT_CALL(http_client_,
              MockSendRequest("POST", dev_reg_->GetOAuthURL("token"), _,
                              "application/x-www-form-urlencoded",
                              HttpClient::Headers{}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_EQ("authorization_code", GetFormField(data, "grant_type"));
        EXPECT_EQ(test_data::kRobotAccountAuthCode, GetFormField(data, "code"));
        EXPECT_EQ(test_data::kClientId, GetFormField(data, "client_id"));
        EXPECT_EQ(test_data::kClientSecret,
                  GetFormField(data, "client_secret"));

        EXPECT_EQ("oob", GetFormField(data, "redirect_uri"));
        EXPECT_EQ("https://www.googleapis.com/auth/clouddevices",
                  GetFormField(data, "scope"));

        base::DictionaryValue json;
        json.SetString("access_token", test_data::kAccessToken);
        json.SetString("token_type", "Bearer");
        json.SetString("refresh_token", test_data::kRefreshToken);
        json.SetInteger("expires_in", 3600);

        return ReplyWithJson(200, json);
      })));

  std::string device_id =
      dev_reg_->RegisterDevice(test_data::kClaimTicketId, nullptr);

  EXPECT_EQ(test_data::kDeviceId, device_id);
  EXPECT_EQ(RegistrationStatus::kConnecting, GetRegistrationStatus());

  // Validate the device info saved to storage...
  auto storage_data = storage_->Load();
  base::DictionaryValue* dict = nullptr;
  EXPECT_TRUE(storage_data->GetAsDictionary(&dict));
  std::string value;
  EXPECT_TRUE(dict->GetString("device_id", &value));
  EXPECT_EQ(test_data::kDeviceId, value);
  EXPECT_TRUE(dict->GetString("refresh_token", &value));
  EXPECT_EQ(test_data::kRefreshToken, value);
  EXPECT_TRUE(dict->GetString("robot_account", &value));
  EXPECT_EQ(test_data::kRobotAccountEmail, value);
}

TEST_F(DeviceRegistrationInfoTest, OOBRegistrationStatus) {
  // After we've been initialized, we should be either offline or
  // unregistered, depending on whether or not we've found credentials.
  EXPECT_EQ(RegistrationStatus::kUnconfigured, GetRegistrationStatus());
  // Put some credentials into our state, make sure we call that offline.
  SetDefaultDeviceRegistration(&data_);
  storage_->Save(data_);
  ReloadConfig();
  EXPECT_EQ(RegistrationStatus::kConnecting, GetRegistrationStatus());
}

TEST_F(DeviceRegistrationInfoTest, UpdateCommand) {
  SetDefaultDeviceRegistration(&data_);
  storage_->Save(data_);
  ReloadConfig();
  SetAccessToken();

  auto json_cmds = CreateDictionaryValue(R"({
    'robot': {
      '_jump': {
        'parameters': {'_height': 'integer'},
        'progress': {'progress': 'integer'},
        'results': {'status': 'string'},
        'minimalRole': 'user'
      }
    }
  })");
  EXPECT_TRUE(command_manager_->LoadCommands(*json_cmds, "", nullptr));

  const std::string command_url = dev_reg_->GetServiceURL("commands/1234");

  auto commands_json = CreateValue(R"([{
    'name':'robot._jump',
    'id':'1234',
    'parameters': {'_height': 100},
    'minimalRole': 'user'
  }])");
  ASSERT_NE(nullptr, commands_json.get());
  const base::ListValue* command_list = nullptr;
  ASSERT_TRUE(commands_json->GetAsList(&command_list));
  PublishCommands(*command_list);
  auto command = command_manager_->FindCommand("1234");
  ASSERT_NE(nullptr, command);

  EXPECT_CALL(http_client_,
              MockSendRequest("PATCH", command_url, _,
                              "application/json; charset=utf-8",
                              HttpClient::Headers{GetAuthHeader()}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_JSON_EQ(R"({"results":{"status":"Ok"}})",
                       *CreateDictionaryValue(data));
        base::DictionaryValue json;
        return ReplyWithJson(200, json);
      })));
  EXPECT_TRUE(
      command->SetResults(*CreateDictionaryValue("{'status': 'Ok'}"), nullptr));
  Mock::VerifyAndClearExpectations(&http_client_);

  EXPECT_CALL(http_client_,
              MockSendRequest("PATCH", command_url, _,
                              "application/json; charset=utf-8",
                              HttpClient::Headers{GetAuthHeader()}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_JSON_EQ(R"({"state":"inProgress"})",
                       *CreateDictionaryValue(data));
        base::DictionaryValue json;
        return ReplyWithJson(200, json);
      })))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_JSON_EQ(R"({"progress":{"progress":18}})",
                       *CreateDictionaryValue(data));
        base::DictionaryValue json;
        return ReplyWithJson(200, json);
      })));
  EXPECT_TRUE(
      command->SetProgress(*CreateDictionaryValue("{'progress':18}"), nullptr));
  Mock::VerifyAndClearExpectations(&http_client_);

  EXPECT_CALL(http_client_,
              MockSendRequest("PATCH", command_url, _,
                              "application/json; charset=utf-8",
                              HttpClient::Headers{GetAuthHeader()}, _))
      .WillOnce(WithArgs<2>(Invoke([](const std::string& data) {
        EXPECT_JSON_EQ(R"({"state":"cancelled"})",
                       *CreateDictionaryValue(data));
        base::DictionaryValue json;
        return ReplyWithJson(200, json);
      })));
  command->Cancel();
  Mock::VerifyAndClearExpectations(&http_client_);
}

}  // namespace weave

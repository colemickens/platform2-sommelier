// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/base_api_handler.h"

#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <gtest/gtest.h>
#include <weave/mock_config_store.h>
#include <weave/mock_http_client.h>

#include "libweave/src/commands/command_manager.h"
#include "libweave/src/commands/unittest_utils.h"
#include "libweave/src/config.h"
#include "libweave/src/device_registration_info.h"
#include "libweave/src/states/mock_state_change_queue_interface.h"
#include "libweave/src/states/state_manager.h"

using testing::_;
using testing::StrictMock;
using testing::Return;

namespace weave {

class BaseApiHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_CALL(mock_state_change_queue_, NotifyPropertiesUpdated(_, _))
        .WillRepeatedly(Return(true));

    command_manager_ = std::make_shared<CommandManager>();
    state_manager_ = std::make_shared<StateManager>(&mock_state_change_queue_);
    auto state_definition = unittests::CreateDictionaryValue(R"({
      'base': {
        'firmwareVersion': 'string',
        'localDiscoveryEnabled': 'boolean',
        'localAnonymousAccessMaxRole': [ 'none', 'viewer', 'user' ],
        'localPairingEnabled': 'boolean',
        'network': {
          'properties': {
            'name': 'string'
          }
        }
      }
    })");
    auto state_defaults = unittests::CreateDictionaryValue(R"({
      'base': {
        'firmwareVersion': '123123',
        'localDiscoveryEnabled': false,
        'localAnonymousAccessMaxRole': 'none',
        'localPairingEnabled': false
      }
    })");
    ASSERT_TRUE(state_manager_->LoadStateDefinition(*state_definition, "base",
                                                    nullptr));
    ASSERT_TRUE(state_manager_->LoadStateDefaults(*state_defaults, nullptr));
    dev_reg_.reset(new DeviceRegistrationInfo(
        command_manager_, state_manager_,
        std::unique_ptr<Config>{new Config{&config_store_}}, nullptr,
        &http_client_, true, nullptr));
    handler_.reset(new BaseApiHandler{dev_reg_.get(), "123123", state_manager_,
                                      command_manager_});
  }

  void LoadCommands(const std::string& command_definitions) {
    auto json = unittests::CreateDictionaryValue(command_definitions.c_str());
    EXPECT_TRUE(command_manager_->LoadBaseCommands(*json, nullptr));
    EXPECT_TRUE(command_manager_->LoadCommands(*json, "", nullptr));
  }

  void AddCommand(const std::string& command) {
    auto command_instance = CommandInstance::FromJson(
        unittests::CreateDictionaryValue(command.c_str()).get(),
        CommandOrigin::kLocal, command_manager_->GetCommandDictionary(),
        nullptr, nullptr);
    EXPECT_TRUE(!!command_instance);

    std::string id{base::IntToString(++command_id_)};
    command_instance->SetID(id);
    command_manager_->AddCommand(std::move(command_instance));
    EXPECT_EQ(CommandStatus::kDone,
              command_manager_->FindCommand(id)->GetStatus());
  }

  unittests::MockConfigStore config_store_;
  StrictMock<unittests::MockHttpClient> http_client_;
  std::unique_ptr<DeviceRegistrationInfo> dev_reg_;
  std::shared_ptr<CommandManager> command_manager_;
  testing::StrictMock<MockStateChangeQueueInterface> mock_state_change_queue_;
  std::shared_ptr<StateManager> state_manager_;
  std::unique_ptr<BaseApiHandler> handler_;
  int command_id_{0};
};

TEST_F(BaseApiHandlerTest, UpdateBaseConfiguration) {
  LoadCommands(R"({
    'base': {
      'updateBaseConfiguration': {
        'parameters': {
          'localDiscoveryEnabled': 'boolean',
          'localAnonymousAccessMaxRole': [ 'none', 'viewer', 'user' ],
          'localPairingEnabled': 'boolean'
         },
         'results': {}
      }
    }
  })");

  Config& config{*dev_reg_->GetMutableConfig()};

  AddCommand(R"({
    'name' : 'base.updateBaseConfiguration',
    'parameters': {
      'localDiscoveryEnabled': false,
      'localAnonymousAccessMaxRole': 'none',
      'localPairingEnabled': false
    }
  })");
  EXPECT_EQ("none", config.local_anonymous_access_role());
  EXPECT_FALSE(config.local_discovery_enabled());
  EXPECT_FALSE(config.local_pairing_enabled());

  auto expected = R"({
    'base': {
      'firmwareVersion': '123123',
      'localAnonymousAccessMaxRole': 'none',
      'localDiscoveryEnabled': false,
      'localPairingEnabled': false,
      'network': {}
    }
  })";
  EXPECT_JSON_EQ(expected, *state_manager_->GetStateValuesAsJson());

  AddCommand(R"({
    'name' : 'base.updateBaseConfiguration',
    'parameters': {
      'localDiscoveryEnabled': true,
      'localAnonymousAccessMaxRole': 'user',
      'localPairingEnabled': true
    }
  })");
  EXPECT_EQ("user", config.local_anonymous_access_role());
  EXPECT_TRUE(config.local_discovery_enabled());
  EXPECT_TRUE(config.local_pairing_enabled());
  expected = R"({
    'base': {
      'firmwareVersion': '123123',
      'localAnonymousAccessMaxRole': 'user',
      'localDiscoveryEnabled': true,
      'localPairingEnabled': true,
      'network': {}
    }
  })";
  EXPECT_JSON_EQ(expected, *state_manager_->GetStateValuesAsJson());

  {
    Config::Transaction change{&config};
    change.set_local_anonymous_access_role("viewer");
  }
  expected = R"({
    'base': {
      'firmwareVersion': '123123',
      'localAnonymousAccessMaxRole': 'viewer',
      'localDiscoveryEnabled': true,
      'localPairingEnabled': true,
      'network': {}
    }
  })";
  EXPECT_JSON_EQ(expected, *state_manager_->GetStateValuesAsJson());
}

TEST_F(BaseApiHandlerTest, UpdateDeviceInfo) {
  LoadCommands(R"({
    'base': {
      'updateDeviceInfo': {
        'parameters': {
          'description': 'string',
          'name': {
            'type': 'string',
            'minLength': 1
          },
          'location': 'string'
        },
        'results': {}
      }
    }
  })");

  AddCommand(R"({
    'name' : 'base.updateDeviceInfo',
    'parameters': {
      'name': 'testName',
      'description': 'testDescription',
      'location': 'testLocation'
    }
  })");

  const Config& config{dev_reg_->GetConfig()};
  EXPECT_EQ("testName", config.name());
  EXPECT_EQ("testDescription", config.description());
  EXPECT_EQ("testLocation", config.location());

  AddCommand(R"({
    'name' : 'base.updateDeviceInfo',
    'parameters': {
      'location': 'newLocation'
    }
  })");

  EXPECT_EQ("testName", config.name());
  EXPECT_EQ("testDescription", config.description());
  EXPECT_EQ("newLocation", config.location());
}

}  // namespace weave

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/manager.h"

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "wimax_manager/event_dispatcher.h"
#include "wimax_manager/proto_bindings/config.pb.h"

using base::FilePath;
using std::string;

namespace wimax_manager {

namespace {

const char kTestConfigFileContent[] =
    "# Test config\n"
    "\n"
    "# Network operator 1\n"
    "network_operator {\n"
    "  identifier: 1\n"
    "}\n"
    "\n"
    "# Network operator 2\n"
    "network_operator {\n"
    "  identifier: 2\n"
    "  eap_parameters {\n"
    "    type: EAP_TYPE_TLS\n"
    "  }\n"
    "}\n"
    "\n"
    "# Network operator 2\n"
    "network_operator {\n"
    "  identifier: 0x00000003\n"
    "  name: \"My Net\"\n"
    "  eap_parameters {\n"
    "    type: EAP_TYPE_TTLS_MSCHAPV2\n"
    "    anonymous_identity: \"test\"\n"
    "    user_identity: \"user\"\n"
    "    user_password: \"password\"\n"
    "    bypass_device_certificate: true\n"
    "    bypass_ca_certificate: true\n"
    "  }\n"
    "}"
    "\n";

}  // namespace

class ManagerTest : public testing::Test {
 protected:
  ManagerTest() : manager_(&dispatcher_) {}

  bool CreateConfigFileInDir(const string& content,
                             const FilePath& dir,
                             FilePath* config_file) {
    if (!base::CreateTemporaryFileInDir(dir, config_file))
      return false;

    if (base::WriteFile(*config_file, content.data(), content.size()) !=
        static_cast<int>(content.size())) {
      return false;
    }

    return true;
  }

  EventDispatcher dispatcher_;
  Manager manager_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(ManagerTest, GetNetworkOperator) {
  // No config is loaded.
  EXPECT_EQ(nullptr, manager_.GetNetworkOperator(0));

  base::FilePath config_file;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(CreateConfigFileInDir(kTestConfigFileContent, temp_dir_.GetPath(),
                                    &config_file));

  EXPECT_TRUE(manager_.LoadConfig(config_file));

  EXPECT_EQ(nullptr, manager_.GetNetworkOperator(0));

  const NetworkOperator* network_operator1 = manager_.GetNetworkOperator(1);
  EXPECT_NE(nullptr, network_operator1);
  EXPECT_EQ(1, network_operator1->identifier());
  EXPECT_EQ("", network_operator1->name());
  EXPECT_EQ(EAP_TYPE_NONE, network_operator1->eap_parameters().type());
  EXPECT_EQ("", network_operator1->eap_parameters().anonymous_identity());
  EXPECT_EQ("", network_operator1->eap_parameters().user_identity());
  EXPECT_EQ("", network_operator1->eap_parameters().user_password());
  EXPECT_FALSE(network_operator1->eap_parameters().bypass_device_certificate());
  EXPECT_FALSE(network_operator1->eap_parameters().bypass_ca_certificate());

  const NetworkOperator* network_operator2 = manager_.GetNetworkOperator(2);
  EXPECT_NE(nullptr, network_operator2);
  EXPECT_EQ(2, network_operator2->identifier());
  EXPECT_EQ("", network_operator2->name());
  EXPECT_EQ(EAP_TYPE_TLS, network_operator2->eap_parameters().type());
  EXPECT_EQ("", network_operator2->eap_parameters().anonymous_identity());
  EXPECT_EQ("", network_operator2->eap_parameters().user_identity());
  EXPECT_EQ("", network_operator2->eap_parameters().user_password());
  EXPECT_FALSE(network_operator2->eap_parameters().bypass_device_certificate());
  EXPECT_FALSE(network_operator2->eap_parameters().bypass_ca_certificate());

  const NetworkOperator* network_operator3 = manager_.GetNetworkOperator(3);
  EXPECT_NE(nullptr, network_operator3);
  EXPECT_EQ(3, network_operator3->identifier());
  EXPECT_EQ("My Net", network_operator3->name());
  EXPECT_EQ(EAP_TYPE_TTLS_MSCHAPV2, network_operator3->eap_parameters().type());
  EXPECT_EQ("test", network_operator3->eap_parameters().anonymous_identity());
  EXPECT_EQ("user", network_operator3->eap_parameters().user_identity());
  EXPECT_EQ("password", network_operator3->eap_parameters().user_password());
  EXPECT_TRUE(network_operator3->eap_parameters().bypass_device_certificate());
  EXPECT_TRUE(network_operator3->eap_parameters().bypass_ca_certificate());
}

TEST_F(ManagerTest, LoadEmptyConfigFile) {
  base::FilePath config_file;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(CreateConfigFileInDir("", temp_dir_.GetPath(), &config_file));

  EXPECT_TRUE(manager_.LoadConfig(config_file));
  Config* config = manager_.config_.get();
  EXPECT_NE(nullptr, config);
  EXPECT_EQ(0, config->network_operator_size());
}

TEST_F(ManagerTest, LoadInvalidConfigFile) {
  base::FilePath config_file;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(CreateConfigFileInDir("<invalid config>", temp_dir_.GetPath(),
                                    &config_file));

  EXPECT_FALSE(manager_.LoadConfig(config_file));
  EXPECT_EQ(nullptr, manager_.config_.get());
}

TEST_F(ManagerTest, LoadNonExistentConfigFile) {
  EXPECT_FALSE(manager_.LoadConfig(FilePath("/non-existent-file")));
  EXPECT_EQ(nullptr, manager_.config_.get());
}

TEST_F(ManagerTest, LoadValidConfigFile) {
  base::FilePath config_file;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(CreateConfigFileInDir(kTestConfigFileContent, temp_dir_.GetPath(),
                                    &config_file));

  EXPECT_TRUE(manager_.LoadConfig(config_file));
  Config* config = manager_.config_.get();
  EXPECT_NE(nullptr, config);
  EXPECT_EQ(3, config->network_operator_size());

  const NetworkOperator& network_operator1 = config->network_operator(0);
  EXPECT_EQ(1, network_operator1.identifier());
  EXPECT_EQ("", network_operator1.name());
  EXPECT_EQ(EAP_TYPE_NONE, network_operator1.eap_parameters().type());
  EXPECT_EQ("", network_operator1.eap_parameters().anonymous_identity());
  EXPECT_EQ("", network_operator1.eap_parameters().user_identity());
  EXPECT_EQ("", network_operator1.eap_parameters().user_password());
  EXPECT_FALSE(network_operator1.eap_parameters().bypass_device_certificate());
  EXPECT_FALSE(network_operator1.eap_parameters().bypass_ca_certificate());

  const NetworkOperator& network_operator2 = config->network_operator(1);
  EXPECT_EQ(2, network_operator2.identifier());
  EXPECT_EQ("", network_operator2.name());
  EXPECT_EQ(EAP_TYPE_TLS, network_operator2.eap_parameters().type());
  EXPECT_EQ("", network_operator2.eap_parameters().anonymous_identity());
  EXPECT_EQ("", network_operator2.eap_parameters().user_identity());
  EXPECT_EQ("", network_operator2.eap_parameters().user_password());
  EXPECT_FALSE(network_operator2.eap_parameters().bypass_device_certificate());
  EXPECT_FALSE(network_operator2.eap_parameters().bypass_ca_certificate());

  const NetworkOperator& network_operator3 = config->network_operator(2);
  EXPECT_EQ(3, network_operator3.identifier());
  EXPECT_EQ("My Net", network_operator3.name());
  EXPECT_EQ(EAP_TYPE_TTLS_MSCHAPV2, network_operator3.eap_parameters().type());
  EXPECT_EQ("test", network_operator3.eap_parameters().anonymous_identity());
  EXPECT_EQ("user", network_operator3.eap_parameters().user_identity());
  EXPECT_EQ("password", network_operator3.eap_parameters().user_password());
  EXPECT_TRUE(network_operator3.eap_parameters().bypass_device_certificate());
  EXPECT_TRUE(network_operator3.eap_parameters().bypass_ca_certificate());
}

}  // namespace wimax_manager

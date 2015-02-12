// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webserver/webservd/config.h"

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <chromeos/errors/error_codes.h>
#include <gtest/gtest.h>

#include "webserver/webservd/error_codes.h"
#include "webserver/webservd/protocol_handler.h"

namespace {

const char kTestConfig[] = R"({
  "protocol_handlers": {
    "ue_p2p": {
      "port": 16725,
      "dummy_data_to_ignore": 123,
    },
  },
  "dummy_data_to_ignore2": "ignore me",
})";

const char kInvalidConfig_NotDict[] = R"({
  "protocol_handlers": {
    "http": "not_a_dict"
  }
})";

const char kInvalidConfig_NoPort[] = R"({
  "protocol_handlers": {
    "http": {
      "use_tls": true
    }
  }
})";

const char kInvalidConfig_InvalidPort[] = R"({
  "protocol_handlers": {
    "https": {
      "port": 65536
    }
  }
})";

void ValidateConfig(const webservd::Config& config) {
  EXPECT_FALSE(config.use_debug);

  ASSERT_EQ(1u, config.protocol_handlers.size());

  auto it = config.protocol_handlers.begin();
  EXPECT_EQ("ue_p2p", it->first);
  EXPECT_EQ(16725u, it->second.port);
  EXPECT_FALSE(it->second.use_tls);
  EXPECT_TRUE(it->second.certificate.empty());
  EXPECT_TRUE(it->second.certificate_fingerprint.empty());
  EXPECT_TRUE(it->second.private_key.empty());
}

}  // anonymous namespace

namespace webservd {

TEST(Config, LoadDefault) {
  Config config;
  LoadDefaultConfig(&config);
  EXPECT_FALSE(config.use_debug);

  ASSERT_EQ(2u, config.protocol_handlers.size());

  const Config::ProtocolHandler& http_config =
      config.protocol_handlers[ProtocolHandler::kHttp];
  EXPECT_EQ(80u, http_config.port);
  EXPECT_FALSE(http_config.use_tls);
  EXPECT_TRUE(http_config.certificate.empty());
  EXPECT_TRUE(http_config.certificate_fingerprint.empty());
  EXPECT_TRUE(http_config.private_key.empty());

  const Config::ProtocolHandler& https_config =
      config.protocol_handlers[ProtocolHandler::kHttps];
  EXPECT_EQ(443u, https_config.port);
  EXPECT_TRUE(https_config.use_tls);

  // TLS keys/certificates are set later in webservd::Server, not on load.
  EXPECT_TRUE(https_config.certificate.empty());
  EXPECT_TRUE(https_config.certificate_fingerprint.empty());
  EXPECT_TRUE(https_config.private_key.empty());
}

TEST(Config, LoadConfigFromString) {
  Config config;
  ASSERT_TRUE(LoadConfigFromString(kTestConfig, &config, nullptr));
  ValidateConfig(config);
}

TEST(Config, LoadConfigFromFile) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  base::FilePath config_path{temp.path().Append("test.config")};
  // For the record: I hate base::WriteFile() and its usage of ints.
  int data_len = sizeof(kTestConfig) - 1;
  ASSERT_EQ(data_len, base::WriteFile(config_path, kTestConfig, data_len));

  Config config;
  LoadConfigFromFile(config_path, &config);
  ValidateConfig(config);
}

TEST(Config, ParseError_ProtocolHandlersNotDict) {
  chromeos::ErrorPtr error;
  Config config;
  ASSERT_FALSE(LoadConfigFromString(kInvalidConfig_NotDict, &config, &error));
  EXPECT_EQ(chromeos::errors::json::kDomain, error->GetDomain());
  EXPECT_EQ(chromeos::errors::json::kObjectExpected, error->GetCode());
  EXPECT_EQ("Protocol handler definition for 'http' must be a JSON object",
            error->GetMessage());
}

TEST(Config, ParseError_NoPort) {
  chromeos::ErrorPtr error;
  Config config;
  ASSERT_FALSE(LoadConfigFromString(kInvalidConfig_NoPort, &config, &error));
  EXPECT_EQ(webservd::errors::kDomain, error->GetDomain());
  EXPECT_EQ(webservd::errors::kInvalidConfig, error->GetCode());
  EXPECT_EQ("Unable to parse config for protocol handler 'http'",
            error->GetMessage());
  EXPECT_EQ(webservd::errors::kDomain, error->GetInnerError()->GetDomain());
  EXPECT_EQ(webservd::errors::kInvalidConfig,
            error->GetInnerError()->GetCode());
  EXPECT_EQ("Port is missing", error->GetInnerError()->GetMessage());
}

TEST(Config, ParseError_InvalidPort) {
  chromeos::ErrorPtr error;
  Config config;
  ASSERT_FALSE(LoadConfigFromString(kInvalidConfig_InvalidPort, &config,
               &error));
  EXPECT_EQ(webservd::errors::kDomain, error->GetDomain());
  EXPECT_EQ(webservd::errors::kInvalidConfig, error->GetCode());
  EXPECT_EQ("Unable to parse config for protocol handler 'https'",
            error->GetMessage());
  EXPECT_EQ(webservd::errors::kDomain, error->GetInnerError()->GetDomain());
  EXPECT_EQ(webservd::errors::kInvalidConfig,
            error->GetInnerError()->GetCode());
  EXPECT_EQ("Invalid port value: 65536", error->GetInnerError()->GetMessage());
}

}  // namespace webservd

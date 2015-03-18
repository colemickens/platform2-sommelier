// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_BUFFET_CONFIG_H_
#define BUFFET_BUFFET_CONFIG_H_

#include <string>

#include <base/files/file_path.h>
#include <chromeos/key_value_store.h>

namespace buffet {

class BuffetConfig {
 public:
  BuffetConfig() = default;

  void Load(const base::FilePath& config_path);
  void Load(const chromeos::KeyValueStore& store);

  std::string client_id() const { return client_id_; }
  std::string client_secret() const { return client_secret_; }
  std::string api_key() const { return api_key_; }
  std::string oauth_url() const { return oauth_url_; }
  std::string service_url() const { return service_url_; }
  std::string device_kind() const { return device_kind_; }
  std::string name() const { return name_; }
  std::string default_display_name() const { return default_display_name_; }
  std::string default_description() const { return default_description_; }
  std::string default_location() const { return default_location_; }
  std::string model_id() const { return model_id_; }
  uint64_t polling_period_ms() const { return polling_period_ms_; }

 private:
  std::string client_id_{"58855907228.apps.googleusercontent.com"};
  std::string client_secret_{"eHSAREAHrIqPsHBxCE9zPPBi"};
  std::string api_key_{"AIzaSyDSq46gG-AxUnC3zoqD9COIPrjolFsMfMA"};
  std::string oauth_url_{"https://accounts.google.com/o/oauth2/"};
  std::string service_url_{"https://www.googleapis.com/clouddevices/v1/"};
  std::string device_kind_{"vendor"};
  std::string name_{"developer_device"};
  std::string default_display_name_{"Developer device"};
  std::string default_description_{"A development device"};
  std::string default_location_{"my desk"};
  std::string model_id_{"AAA"};
  uint64_t polling_period_ms_{7000};

  DISALLOW_COPY_AND_ASSIGN(BuffetConfig);
};

}  // namespace buffet

#endif  // BUFFET_BUFFET_CONFIG_H_

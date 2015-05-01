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

  const std::string& client_id() const { return client_id_; }
  const std::string& client_secret() const { return client_secret_; }
  const std::string& api_key() const { return api_key_; }
  const std::string& oauth_url() const { return oauth_url_; }
  const std::string& service_url() const { return service_url_; }
  const std::string& oem_name() const { return oem_name_; }
  const std::string& model_name() const { return model_name_; }
  const std::string& model_id() const { return model_id_; }
  const std::string& device_kind() const { return device_kind_; }
  uint64_t polling_period_ms() const { return polling_period_ms_; }

  const std::string& name() const { return name_; }
  const std::string& description() const { return description_; }
  const std::string& location() const { return location_; }
  std::string anonymous_access_role() const { return anonymous_access_role_; }

  void set_name(const std::string& name);
  void set_description(const std::string& description) {
    description_ = description;
  }
  void set_location(const std::string& location) { location_ = location; }
  void set_anonymous_access_role(const std::string& role);

 private:
  std::string client_id_{"58855907228.apps.googleusercontent.com"};
  std::string client_secret_{"eHSAREAHrIqPsHBxCE9zPPBi"};
  std::string api_key_{"AIzaSyDSq46gG-AxUnC3zoqD9COIPrjolFsMfMA"};
  std::string oauth_url_{"https://accounts.google.com/o/oauth2/"};
  std::string service_url_{"https://www.googleapis.com/clouddevices/v1/"};
  std::string name_{"Developer device"};
  std::string description_;
  std::string location_;
  std::string anonymous_access_role_{"viewer"};
  std::string oem_name_{"Chromium"};
  std::string model_name_{"Brillo"};
  std::string model_id_{"AAAAA"};
  std::string device_kind_{"vendor"};
  uint64_t polling_period_ms_{7000};

  DISALLOW_COPY_AND_ASSIGN(BuffetConfig);
};

}  // namespace buffet

#endif  // BUFFET_BUFFET_CONFIG_H_

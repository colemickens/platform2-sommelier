// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_CONFIG_H_
#define LIBWEAVE_INCLUDE_WEAVE_CONFIG_H_

#include <set>
#include <string>

#include <base/callback.h>

#include "weave/enum_to_string.h"
#include "weave/privet.h"

namespace weave {

struct Settings {
  std::string client_id;
  std::string client_secret;
  std::string api_key;
  std::string oauth_url;
  std::string service_url;
  std::string name;
  std::string description;
  std::string location;
  std::string local_anonymous_access_role;
  bool local_discovery_enabled{true};
  bool local_pairing_enabled{true};
  std::string oem_name;
  std::string model_name;
  std::string model_id;
  std::string device_kind;
  base::TimeDelta polling_period;
  base::TimeDelta backup_polling_period;

  bool wifi_auto_setup_enabled{true};
  std::set<PairingType> pairing_modes;
  base::FilePath embedded_code_path;

  std::string device_id;
  std::string refresh_token;
  std::string robot_account;
  std::string last_configured_ssid;
};

class Config {
 public:
  using OnChangedCallback = base::Callback<void(const Settings& settings)>;

  // Sets callback which is called when config is changed.
  virtual void AddOnChangedCallback(const OnChangedCallback& callback) = 0;

  virtual const Settings& GetSettings() const = 0;

 protected:
  virtual ~Config() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_CONFIG_H_

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_CONFIG_H_
#define LIBWEAVE_SRC_CONFIG_H_

#include <set>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <weave/error.h>
#include <chromeos/key_value_store.h>
#include <weave/config.h>

#include "libweave/src/privet/security_delegate.h"

namespace weave {

class StorageInterface;

// Handles reading buffet config and state files.
class Config final : public Config {
 public:
  using OnChangedCallback = base::Callback<void(const Settings&)>;
  ~Config() override = default;

  explicit Config(std::unique_ptr<StorageInterface> storage);

  explicit Config(const base::FilePath& state_path);

  // Config overrides.
  void AddOnChangedCallback(const OnChangedCallback& callback) override;
  const Settings& GetSettings() const override;

  void Load(const base::FilePath& config_path);
  void Load(const chromeos::KeyValueStore& store);

  // Allows editing of config. Makes sure that callbacks were called and changes
  // were saved.
  // User can commit changes by calling Commit method or by destroying the
  // object.
  class Transaction final {
   public:
    explicit Transaction(Config* config)
        : config_(config), settings_(&config->settings_) {
      CHECK(config_);
    }

    ~Transaction();

    void set_client_id(const std::string& id) { settings_->client_id = id; }
    void set_client_secret(const std::string& secret) {
      settings_->client_secret = secret;
    }
    void set_api_key(const std::string& key) { settings_->api_key = key; }
    void set_oauth_url(const std::string& url) { settings_->oauth_url = url; }
    void set_service_url(const std::string& url) {
      settings_->service_url = url;
    }
    void set_name(const std::string& name) { settings_->name = name; }
    void set_description(const std::string& description) {
      settings_->description = description;
    }
    void set_location(const std::string& location) {
      settings_->location = location;
    }
    bool set_local_anonymous_access_role(const std::string& role);
    void set_local_discovery_enabled(bool enabled) {
      settings_->local_discovery_enabled = enabled;
    }
    void set_local_pairing_enabled(bool enabled) {
      settings_->local_pairing_enabled = enabled;
    }
    void set_device_id(const std::string& id) { settings_->device_id = id; }
    void set_refresh_token(const std::string& token) {
      settings_->refresh_token = token;
    }
    void set_robot_account(const std::string& account) {
      settings_->robot_account = account;
    }
    void set_last_configured_ssid(const std::string& ssid) {
      settings_->last_configured_ssid = ssid;
    }

    void Commit();

   private:
    friend class Config;
    void LoadState();
    Config* config_;
    Settings* settings_;
    bool save_{true};
  };

  const std::string& client_id() const { return settings_.client_id; }
  const std::string& client_secret() const { return settings_.client_secret; }
  const std::string& api_key() const { return settings_.api_key; }
  const std::string& oauth_url() const { return settings_.oauth_url; }
  const std::string& service_url() const { return settings_.service_url; }
  const std::string& oem_name() const { return settings_.oem_name; }
  const std::string& model_name() const { return settings_.model_name; }
  const std::string& model_id() const { return settings_.model_id; }
  base::TimeDelta polling_period() const { return settings_.polling_period; }
  base::TimeDelta backup_polling_period() const {
    return settings_.backup_polling_period;
  }

  bool wifi_auto_setup_enabled() const {
    return settings_.wifi_auto_setup_enabled;
  }
  bool ble_setup_enabled() const { return settings_.ble_setup_enabled; }
  const std::set<PairingType>& pairing_modes() const {
    return settings_.pairing_modes;
  }
  const std::string& embedded_code() const { return settings_.embedded_code; }

  const std::string& name() const { return settings_.name; }
  const std::string& description() const { return settings_.description; }
  const std::string& location() const { return settings_.location; }
  const std::string& local_anonymous_access_role() const {
    return settings_.local_anonymous_access_role;
  }
  bool local_pairing_enabled() const { return settings_.local_pairing_enabled; }
  bool local_discovery_enabled() const {
    return settings_.local_discovery_enabled;
  }

  const std::string& device_id() const { return settings_.device_id; }
  const std::string& refresh_token() const { return settings_.refresh_token; }
  const std::string& robot_account() const { return settings_.robot_account; }
  const std::string& last_configured_ssid() const {
    return settings_.last_configured_ssid;
  }

 private:
  bool Save();
  static Settings CreateDefaultSettings();

  Settings settings_ = CreateDefaultSettings();

  // Serialization interface to save and load buffet state.
  std::unique_ptr<StorageInterface> storage_;

  std::vector<OnChangedCallback> on_changed_;

  DISALLOW_COPY_AND_ASSIGN(Config);
};

}  // namespace weave

#endif  // LIBWEAVE_SRC_CONFIG_H_

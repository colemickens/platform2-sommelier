// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_INCLUDE_WEAVE_CLOUD_H_
#define LIBWEAVE_INCLUDE_WEAVE_CLOUD_H_

#include <string>

#include <base/callback.h>
#include <base/values.h>
#include <chromeos/errors/error.h>

namespace weave {

// See the DBus interface XML file for complete descriptions of these states.
enum class RegistrationStatus {
  kUnconfigured,        // We have no credentials.
  kConnecting,          // We have credentials but not yet connected.
  kConnected,           // We're registered and connected to the cloud.
  kInvalidCredentials,  // Our registration has been revoked.
};

class Cloud {
 public:
  using OnRegistrationChangedCallback =
      base::Callback<void(RegistrationStatus satus)>;
  using OnCloudRequestCallback =
      base::Callback<void(const base::DictionaryValue& response)>;
  using OnCloudRequestErrorCallback =
      base::Callback<void(const chromeos::Error* error)>;

  // Sets callback which is called when registration state is changed.
  virtual void AddOnRegistrationChangedCallback(
      const OnRegistrationChangedCallback& callback) = 0;

  // Gets the full device description JSON object asynchronously.
  // Passes the device info as the first argument to |callback|, or nullptr if
  // the device is not registered or in case of communication failure.
  virtual void GetDeviceInfo(
      const OnCloudRequestCallback& success_callback,
      const OnCloudRequestErrorCallback& error_callback) = 0;

  // Registers the device.
  // Returns a device ID on success.
  virtual std::string RegisterDevice(const std::string& ticket_id,
                                     chromeos::ErrorPtr* error) = 0;

  // Updates basic device information.
  virtual bool UpdateDeviceInfo(const std::string& name,
                                const std::string& description,
                                const std::string& location,
                                chromeos::ErrorPtr* error) = 0;

  // Updates base device config.
  virtual bool UpdateBaseConfig(const std::string& anonymous_access_role,
                                bool local_discovery_enabled,
                                bool local_pairing_enabled,
                                chromeos::ErrorPtr* error) = 0;

  // Updates GCD service configuration. Usually for testing.
  virtual bool UpdateServiceConfig(const std::string& client_id,
                                   const std::string& client_secret,
                                   const std::string& api_key,
                                   const std::string& oauth_url,
                                   const std::string& service_url,
                                   chromeos::ErrorPtr* error) = 0;

 protected:
  virtual ~Cloud() = default;
};

}  // namespace weave

#endif  // LIBWEAVE_INCLUDE_WEAVE_CLOUD_H_

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BUFFET_DEVICE_INFO_H_
#define BUFFET_DEVICE_INFO_H_

#include <base/basictypes.h>
#include <base/time/time.h>
#include <string>
#include <map>
#include <memory>

#include "buffet/data_encoding.h"

namespace base {
  class Value;
}  // namespace base

namespace buffet {

// The DeviceRegistrationInfo class represents device registration information.
  class DeviceRegistrationInfo {
 public:
   DeviceRegistrationInfo() = default;

  // Returns the authorization HTTP header that can be used to talk
  // to GCD server for authenticated device communication.
  // Make sure CheckRegistration() is called before this call.
  std::pair<std::string, std::string> GetAuthorizationHeader() const;

  // Returns the GCD service request URL. If |subpath| is specified, it is
  // appended to the base URL which is normally
  //    https://www.googleapis.com/clouddevices/v1/".
  // If |params| are specified, each key-value pair is formatted using
  // chromeos::data_encoding::WebParamsEncode() and appended to URL as a query
  // string.
  // So, calling:
  //    GetServiceURL("ticket", {{"key","apiKey"}})
  // will return something like:
  //    https://www.googleapis.com/clouddevices/v1/ticket?key=apiKey
  std::string GetServiceURL(
      const std::string& subpath = {},
      const chromeos::data_encoding::WebParamList& params = {}) const;

  // Returns a service URL to access the registered device on GCD server.
  // The base URL used to construct the full URL looks like this:
  //    https://www.googleapis.com/clouddevices/v1/devices/<device_id>/
  std::string GetDeviceURL(
    const std::string& subpath = {},
    const chromeos::data_encoding::WebParamList& params = {}) const;

  // Similar to GetServiceURL, GetOAuthURL() returns a URL of OAuth 2.0 server.
  // The base URL used is https://accounts.google.com/o/oauth2/.
  std::string GetOAuthURL(
    const std::string& subpath = {},
    const chromeos::data_encoding::WebParamList& params = {}) const;

  // Returns the registered device ID (GUID) or empty string if failed
  std::string GetDeviceId();

  // Loads the device registration information from cache.
  bool Load();

  // Checks for the valid device registration as well as refreshes
  // the device access token, if available.
  bool CheckRegistration();

  // Gets the full device description JSON object, or nullptr if
  // the device is not registered or communication failure.
  std::unique_ptr<base::Value> GetDeviceInfo();

  // Starts device registration procedure. |params| are a list of
  // key-value pairs of device information, such as client_id, client_secret,
  // and so on. If a particular key-value pair is omitted, a default value
  // is used when possible. Returns a device claim ID on success.
  std::string StartRegistration(
    const std::map<std::string, std::shared_ptr<base::Value>>& params,
    std::string* error_msg);

  // Finalizes the device registration. If |user_auth_code| is provided, then
  // the device record is populated with user email on user's behalf. Otherwise
  // the user is responsible to issue a PATCH request to provide a valid
  // email address before calling FinishRegistration.
  bool FinishRegistration(const std::string& user_auth_code);

 private:
  // Saves the device registration to cache.
  bool Save() const;

  // Makes sure the access token is available and up-to-date.
  bool ValidateAndRefreshAccessToken();

  // Persistent data. Some of default values for testing purposes are used.
  // TODO(avakulenko): remove these default values in the future.
  // http://crbug.com/364692
  std::string client_id_ =
    "583509257718-lnmeofvjef3b1tm33sbjmckfnumfvn8j.apps.googleusercontent.com";
  std::string client_secret_ = "6fzZwQhgnsHhvYYvvFdpv5SD";
  std::string api_key_ = "AIzaSyAp7KVig5m9g4LWWKr79mTS8sXWfUU6w9g";
  std::string refresh_token_;
  std::string device_id_;
  std::string device_robot_account_;
  std::string oauth_url_ = "https://accounts.google.com/o/oauth2/";
  std::string service_url_ =
    "https://www-googleapis-staging.sandbox.google.com/"
    "clouddevices/v1/";

  // Transient data
  std::string access_token_;
  base::Time access_token_expiration_;
  std::string ticket_id_;
  std::string device_kind_ = "vendor";
  std::string system_name_ = "coffee_pot";
  std::string display_name_ = "Coffee Pot";

  DISALLOW_COPY_AND_ASSIGN(DeviceRegistrationInfo);
};

}  // namespace buffet

#endif  // BUFFET_DEVICE_INFO_H_

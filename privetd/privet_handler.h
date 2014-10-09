// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_PRIVET_HANDLER_H_
#define PRIVETD_PRIVET_HANDLER_H_

#include <map>
#include <string>
#include <utility>

#include <base/callback_forward.h>
#include <base/values.h>

namespace privetd {

class CloudDelegate;
class DeviceDelegate;
class SecurityDelegate;
class WifiDelegate;

enum class AuthScope;

// Privet V3 HTTP/HTTPS requests handler.
// API details at https://developers.google.com/cloud-devices/
class PrivetHandler {
 public:
  // Callback to handle requests asynchronously.
  // |success| is true if request handled successfully.
  // |output| is result returned in HTTP response. Contains result of
  // successfully request of information about error.
  using RequestCallback =
      base::Callback<void(const base::DictionaryValue& output, bool success)>;

  PrivetHandler(CloudDelegate* cloud,
                DeviceDelegate* device,
                SecurityDelegate* pairing,
                WifiDelegate* wifi);
  ~PrivetHandler();

  // Handles HTTP/HTTPS Privet request.
  // |api| is the path from the HTTP request, e.g /privet/info.
  // |auth_header| is the Authentication header from HTTP request.
  // |input| is the the POST data from HTTP request.
  // |callback| is result callback which is called iff method returned true.
  // If API is not available method returns false and |callback| will no be
  // called. If true was returned, |callback| will be called exactly once during
  // or after |HandleRequest| call.
  bool HandleRequest(const std::string& api,
                     const std::string& auth_header,
                     const base::DictionaryValue& input,
                     const RequestCallback& callback);

 private:
  using ApiHandler = base::Callback<bool(const base::DictionaryValue&,
                                         const RequestCallback&)>;

  bool ReturnError(const std::string& error,
                   const RequestCallback& callback) const;
  bool ReturnErrorWithMessage(const std::string& error,
                              const std::string& message,
                              const RequestCallback& callback) const;

  bool HandleInfo(const base::DictionaryValue&,
                  const RequestCallback& callback);
  bool HandleAuth(const base::DictionaryValue& input,
                  const RequestCallback& callback);
  bool HandleSetupStart(const base::DictionaryValue& input,
                        const RequestCallback& callback);
  bool HandleSetupStatus(const base::DictionaryValue& input,
                         const RequestCallback& callback);

  CloudDelegate* cloud_ = nullptr;
  DeviceDelegate* device_ = nullptr;
  SecurityDelegate* security_ = nullptr;
  WifiDelegate* wifi_ = nullptr;

  std::map<std::string, std::pair<AuthScope, ApiHandler>> handlers_;
};

}  // namespace privetd

#endif  // PRIVETD_PRIVET_HANDLER_H_

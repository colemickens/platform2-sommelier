// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_DEV_FEATURES_TOOL_H_
#define DEBUGD_SRC_DEV_FEATURES_TOOL_H_

#include <string>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

namespace debugd {

// A collection of functions to enable various development features.
//
// Each feature has two associated functions, one to enable the feature and one
// to query whether it's been enabled or not.
class DevFeaturesTool {
 public:
  DevFeaturesTool() = default;
  ~DevFeaturesTool() = default;

  bool RemoveRootfsVerification(DBus::Error* error) const;
  bool EnableBootFromUsb(DBus::Error* error) const;
  bool ConfigureSshServer(DBus::Error* error) const;
  bool SetUserPassword(const std::string& username,
                       const std::string& password,
                       DBus::Error* error) const;
  bool EnableChromeRemoteDebugging(DBus::Error* error) const;
  bool EnableChromeDevFeatures(const std::string& root_password,
                               DBus::Error* error) const;

  bool QueryDevFeatures(int32_t* flags, DBus::Error* error) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevFeaturesTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_DEV_FEATURES_TOOL_H_

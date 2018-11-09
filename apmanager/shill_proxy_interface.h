// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SHILL_PROXY_INTERFACE_H_
#define APMANAGER_SHILL_PROXY_INTERFACE_H_

#include <string>

namespace apmanager {

class ShillProxyInterface {
 public:
  virtual ~ShillProxyInterface() {}

  // Claim the given interface |interface_name| from shill.
  virtual bool ClaimInterface(const std::string& interface_name) = 0;
  // Release the given interface |interface_name| to shill.
  virtual bool ReleaseInterface(const std::string& interface_name) = 0;
#if defined(__BRILLO__)
  // Setup an AP mode interface.
  virtual bool SetupApModeInterface(std::string* interface_name) = 0;
  // Setup a station mode interface.
  virtual bool SetupStationModeInterface(std::string* interface_name) = 0;
#endif  // __BRILLO__
};

}  // namespace apmanager

#endif  // APMANAGER_SHILL_PROXY_INTERFACE_H_

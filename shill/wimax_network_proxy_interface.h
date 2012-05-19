// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_WIMAX_NETWORK_PROXY_INTERFACE_H_
#define SHILL_WIMAX_NETWORK_PROXY_INTERFACE_H_

#include <string>

#include <base/basictypes.h>
#include <dbus-c++/dbus.h>

namespace shill {

class Error;

// These are the methods that a WiMaxManager.Network proxy must support. The
// interface is provided so that it can be mocked in tests.
class WiMaxNetworkProxyInterface {
 public:
  virtual ~WiMaxNetworkProxyInterface() {}

  virtual DBus::Path proxy_object_path() const = 0;

  // Properties.
  virtual uint32 Identifier(Error *error) = 0;
  virtual std::string Name(Error *error) = 0;
  virtual int Type(Error *error) = 0;
  virtual int CINR(Error *error) = 0;
  virtual int RSSI(Error *error) = 0;
};

}  // namespace shill

#endif  // SHILL_WIMAX_NETWORK_PROXY_INTERFACE_H_

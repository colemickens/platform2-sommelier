// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHAPS_PROXY_INTERFACE_H
#define CHAPS_PROXY_INTERFACE_H

#include <string>

#include <base/basictypes.h>

namespace chaps {

// ChapsProxyInterface provides an abstract interface to the proxy to
// facilitate mocking. It is based on the dbus-c++ generated interface. See
// chaps_interface.xml.
class ChapsProxyInterface {
public:
  ChapsProxyInterface() {}
  virtual ~ChapsProxyInterface() {}

  virtual bool Connect(const std::string& username) = 0;
  virtual void Disconnect() = 0;

private:
  DISALLOW_COPY_AND_ASSIGN(ChapsProxyInterface);
};

}  // namespace

#endif  // CHAPS_PROXY_INTERFACE_H


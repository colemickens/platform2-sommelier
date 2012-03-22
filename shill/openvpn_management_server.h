// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_OPENVPN_MANAGEMENT_SERVER_
#define SHILL_OPENVPN_MANAGEMENT_SERVER_

#include <base/basictypes.h>

namespace shill {

class OpenVPNDriver;

class OpenVPNManagementServer {
 public:
  OpenVPNManagementServer(OpenVPNDriver *driver);
  virtual ~OpenVPNManagementServer();

  // Returns true on success, false on failure.
  bool Init();

 private:
  OpenVPNDriver *driver_;

  DISALLOW_COPY_AND_ASSIGN(OpenVPNManagementServer);
};

}  // namespace shill

#endif  // SHILL_OPENVPN_MANAGEMENT_SERVER_

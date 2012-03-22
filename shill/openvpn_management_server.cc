// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/openvpn_management_server.h"

#include <base/logging.h>

namespace shill {

OpenVPNManagementServer::OpenVPNManagementServer(OpenVPNDriver *driver)
    : driver_(driver) {}

OpenVPNManagementServer::~OpenVPNManagementServer() {}

bool OpenVPNManagementServer::Init() {
  VLOG(2) << __func__;
  return false;
}

}  // namespace shill

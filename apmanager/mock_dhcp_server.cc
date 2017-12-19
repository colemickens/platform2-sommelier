// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_dhcp_server.h"

namespace apmanager {

MockDHCPServer::MockDHCPServer() : DHCPServer(0, "") {}

MockDHCPServer::~MockDHCPServer() {}

}  // namespace apmanager

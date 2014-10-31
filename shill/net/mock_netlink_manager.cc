// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/mock_netlink_manager.h"

#include <string>

#include <base/stl_util.h>

#include "shill/net/netlink_message.h"

using std::string;

namespace shill {

MockNetlinkManager::MockNetlinkManager() {}
MockNetlinkManager::~MockNetlinkManager() {}

}  // namespace shill

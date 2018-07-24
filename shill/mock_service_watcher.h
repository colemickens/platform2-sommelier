// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SERVICE_WATCHER_H_
#define SHILL_MOCK_SERVICE_WATCHER_H_

#include "shill/rpc_service_watcher_interface.h"

namespace shill {

class MockServiceWatcher : public RPCServiceWatcherInterface {
 public:
  MockServiceWatcher() {}
  ~MockServiceWatcher() override {}
};

}  // namespace shill

#endif  // SHILL_MOCK_SERVICE_WATCHER_H_

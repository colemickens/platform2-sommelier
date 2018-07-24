// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RPC_SERVICE_WATCHER_INTERFACE_H_
#define SHILL_RPC_SERVICE_WATCHER_INTERFACE_H_

namespace shill {

// Interface for monitoring a remote RPC service.
class RPCServiceWatcherInterface {
 public:
  virtual ~RPCServiceWatcherInterface() {}
};

}  // namespace shill

#endif  // SHILL_RPC_SERVICE_WATCHER_INTERFACE_H_

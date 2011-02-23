// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include "shill/resource.h"
#include "shill/shill_event.h"

namespace shill {

class ManagerProxyInterface;

class Manager : public Resource {
 public:
  // A constructor for the Manager object
  explicit Manager(ControlInterface *control_interface,
		   EventDispatcher *dispatcher);
  ~Manager();
  void Start();
  void Stop();

 private:
  ManagerProxyInterface *proxy_;
  bool running_;
  friend class ManagerProxyInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_

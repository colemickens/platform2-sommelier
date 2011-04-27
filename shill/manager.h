// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include <vector>

#include "shill/resource.h"
#include "shill/shill_event.h"
#include "shill/service.h"
#include "shill/device.h"

namespace shill {

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
  std::vector<Device*> devices_;
  std::vector<Service*> services_;
  friend class ManagerProxyInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_

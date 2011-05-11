// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MANAGER_
#define SHILL_MANAGER_

#include <vector>
#include <base/scoped_ptr.h>

#include "shill/resource.h"
#include "shill/shill_event.h"
#include "shill/service.h"
#include "shill/device.h"
#include "shill/device_info.h"

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
  scoped_ptr<ManagerAdaptorInterface> adaptor_;
  DeviceInfo device_info_;
  bool running_;
  std::vector<Device*> devices_;
  std::vector<Service*> services_;
  friend class ManagerAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_MANAGER_

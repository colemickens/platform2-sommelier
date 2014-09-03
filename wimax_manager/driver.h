// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DRIVER_H_
#define WIMAX_MANAGER_DRIVER_H_

#include <vector>

#include <base/macros.h>

namespace wimax_manager {

class Device;
class Manager;

class Driver {
 public:
  explicit Driver(Manager *manager);
  virtual ~Driver() = default;

  virtual bool Initialize() = 0;
  virtual bool Finalize() = 0;
  virtual bool GetDevices(std::vector<Device *> *devices) = 0;

 protected:
  Manager *manager() const { return manager_; }

 private:
  Manager *manager_;

  DISALLOW_COPY_AND_ASSIGN(Driver);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DRIVER_H_

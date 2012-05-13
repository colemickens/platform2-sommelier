// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DRIVER_H_
#define WIMAX_MANAGER_DRIVER_H_

#include <vector>

#include <base/basictypes.h>

namespace wimax_manager {

class Device;

class Driver {
 public:
  Driver();
  virtual ~Driver();

  virtual bool Initialize() = 0;
  virtual bool Finalize() = 0;
  virtual bool GetDevices(std::vector<Device *> *devices) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Driver);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DRIVER_H_

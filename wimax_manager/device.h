// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DEVICE_H_
#define WIMAX_MANAGER_DEVICE_H_

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

namespace wimax_manager {

class DeviceDBusAdaptor;

class Device : public base::RefCounted<Device> {
 public:
  Device();

 private:
  friend class base::RefCounted<Device>;

  virtual ~Device();

  scoped_ptr<DeviceDBusAdaptor> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DEVICE_H_

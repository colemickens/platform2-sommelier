// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_CONFIG_INTERFACE_
#define SHILL_DEVICE_CONFIG_INTERFACE_

#include <base/memory/ref_counted.h>

namespace shill {
class DeviceConfigInterface;

typedef scoped_refptr<DeviceConfigInterface> DeviceConfigInterfaceRefPtr;

// The interface by which outside parties can push configuration information
// to devices.
class DeviceConfigInterface : public base::RefCounted<DeviceConfigInterface> {
 public:
  DeviceConfigInterface() {}
  virtual ~DeviceConfigInterface() {}

  // Set IP configuration info.
  // TODO(cmasone): figure out what data needs to be passed in here.
  virtual void ConfigIP() = 0;
 private:
  friend class base::RefCounted<DeviceConfigInterface>;
  DISALLOW_COPY_AND_ASSIGN(DeviceConfigInterface);
};

}  // namespace shill

#endif  // SHILL_DEVICE_CONFIG_INTERFACE_

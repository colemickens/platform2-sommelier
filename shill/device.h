// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_
#define SHILL_DEVICE_

#include <base/ref_counted.h>

#include "shill/shill_event.h"

namespace shill {

// Device superclass.  Individual network interfaces types will inherit from
// this class.
class Device : public base::RefCounted<Device> {
 public:
  // A constructor for the Device object
  explicit Device(ControlInterface *control_interface,
		  EventDispatcher *dispatcher);

  void Start();
  void Stop();

 protected:
  virtual ~Device();

 private:
  DeviceAdaptorInterface *adaptor_;
  bool running_;
  friend class DeviceAdaptorInterface;
};

}  // namespace shill

#endif  // SHILL_DEVICE_

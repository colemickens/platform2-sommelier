// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBMEMS_IIO_CONTEXT_H_
#define LIBMEMS_IIO_CONTEXT_H_

#include <string>

#include <base/macros.h>

#include "libmems/export.h"

namespace libmems {

class IioDevice;

// The IioContext is the root of the tree of IIO devices on the system.
// A context is - at its core - a container of devices, which can be
// retrieved via GetDevice(), providing the device's name as input.
class LIBMEMS_EXPORT IioContext {
 public:
  virtual ~IioContext() = default;

  // libiio loads the device list at context creation time, and does not
  // have a way to update it as new devices appear on the system.
  // This is a helper that allows a rescan of the system to find new devices
  // dynamically at runtime. It should be called after any actions that cause
  // new devices of interest to show up.
  virtual void Reload() = 0;

  // Returns an IioDevice given the device's name or ID.
  // Returns nullptr if the device cannot be found.
  // The device object is guaranteed to stay valid as long as
  // this context object is valid.
  virtual IioDevice* GetDevice(const std::string& name) = 0;

 protected:
  IioContext() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(IioContext);
};

}  // namespace libmems

#endif  // LIBMEMS_IIO_CONTEXT_H_

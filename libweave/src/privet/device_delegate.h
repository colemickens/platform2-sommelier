// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_DEVICE_DELEGATE_H_
#define LIBWEAVE_SRC_PRIVET_DEVICE_DELEGATE_H_

#include <memory>
#include <utility>

#include <base/time/time.h>

namespace weave {
namespace privet {

// Interface to provide access to general information about device.
class DeviceDelegate {
 public:
  DeviceDelegate();
  virtual ~DeviceDelegate();

  // Returns HTTP ports for Privet. The first one is the primary port,
  // the second is the port for a pooling updates requests. The second value
  // could be 0. In this case the first port would be use for regular and for
  // updates requests.
  virtual std::pair<uint16_t, uint16_t> GetHttpEnpoint() const = 0;

  // The same |GetHttpEnpoint| but for HTTPS.
  virtual std::pair<uint16_t, uint16_t> GetHttpsEnpoint() const = 0;

  // Returns device update.
  virtual base::TimeDelta GetUptime() const = 0;

  // Updates the HTTP port value.
  virtual void SetHttpPort(uint16_t port) = 0;

  // Updates the HTTPS port value.
  virtual void SetHttpsPort(uint16_t port) = 0;

  // Create default instance.
  static std::unique_ptr<DeviceDelegate> CreateDefault();
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_DEVICE_DELEGATE_H_

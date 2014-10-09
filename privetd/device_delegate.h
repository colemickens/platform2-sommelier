// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_DEVICE_DELEGATE_H_
#define PRIVETD_DEVICE_DELEGATE_H_

#include <string>
#include <utility>
#include <vector>

#include <base/time/time.h>

namespace privetd {

// Interface to provide access to general information about device.
class DeviceDelegate {
 public:
  DeviceDelegate();
  virtual ~DeviceDelegate();

  // Returns unique id of device. e.g MAC address.
  virtual std::string GetId() const = 0;

  // Returns the name of device. Could be default of set by user.
  virtual std::string GetName() const = 0;

  // Returns the description of the device.
  virtual std::string GetDescription() const = 0;

  // Returns the location of the device.
  virtual std::string GetLocation() const = 0;

  // Returns the list of types supported by device. E.g. printer, scanner etc.
  virtual std::vector<std::string> GetTypes() const = 0;

  // Returns HTTP ports for Privet. The first one is the primary port,
  // the second is the port for a pooling updates requests. The second value
  // could be 0. In this case the first port would be use for regular and for
  // updates requests.
  virtual std::pair<int, int> GetHttpEnpoint() const = 0;

  // The same |GetHttpEnpoint| but for HTTPS.
  virtual std::pair<int, int> GetHttpsEnpoint() const = 0;

  // Returns device update.
  virtual base::TimeDelta GetUptime() const = 0;

  // Sets the name of the device.
  virtual void SetName(const std::string& name) = 0;

  // Sets the name for the device.
  virtual void SetDescription(const std::string& description) = 0;

  // Sets the location of the the device.
  virtual void SetLocation(const std::string& location) = 0;
};

}  // namespace privetd

#endif  // PRIVETD_DEVICE_DELEGATE_H_

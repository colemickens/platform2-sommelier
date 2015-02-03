// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_DEVICE_DELEGATE_H_
#define PRIVETD_DEVICE_DELEGATE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/callback.h>
#include <base/time/time.h>

namespace privetd {

class DaemonState;
class PeerdClient;
class PrivetdConfigParser;

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

  // Returns the class of the device.
  virtual std::string GetClass() const = 0;

  // Returns the model ID of the device.
  virtual std::string GetModelId() const = 0;

  // Returns the list of services supported by device.
  // E.g. printer, scanner etc. Should match services published on mDNS.
  virtual std::vector<std::string> GetServices() const = 0;

  // Returns HTTP ports for Privet. The first one is the primary port,
  // the second is the port for a pooling updates requests. The second value
  // could be 0. In this case the first port would be use for regular and for
  // updates requests.
  virtual std::pair<uint16_t, uint16_t> GetHttpEnpoint() const = 0;

  // The same |GetHttpEnpoint| but for HTTPS.
  virtual std::pair<uint16_t, uint16_t> GetHttpsEnpoint() const = 0;

  // Returns device update.
  virtual base::TimeDelta GetUptime() const = 0;

  // Sets the name of the device.
  virtual void SetName(const std::string& name) = 0;

  // Sets the name for the device.
  virtual void SetDescription(const std::string& description) = 0;

  // Sets the location of the device.
  virtual void SetLocation(const std::string& location) = 0;

  // Updates the HTTP port value.
  virtual void SetHttpPort(uint16_t port) = 0;

  // Updates the HTTPS port value.
  virtual void SetHttpsPort(uint16_t port) = 0;

  // Create default instance.
  static std::unique_ptr<DeviceDelegate> CreateDefault(
      PrivetdConfigParser* config,
      DaemonState* state_store,
      // Allows owner to know that state of the object was changed. Used to
      // notify PeerdClient.
      const base::Closure& on_changed);
};

}  // namespace privetd

#endif  // PRIVETD_DEVICE_DELEGATE_H_

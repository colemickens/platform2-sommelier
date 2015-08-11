// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_CLAIMER_H_
#define SHILL_DEVICE_CLAIMER_H_

#include <set>
#include <string>

#include <base/callback.h>

#include "shill/error.h"
#include "shill/rpc_service_watcher_interface.h"

namespace shill {

class ControlInterface;
class DeviceInfo;

// Provide an abstraction for remote service to claim/release devices
// from/to shill. When the service name is provided, which means that
// the remote service is a RPC endpoint, Shill will perform RPC monitoring
// on that service, and revert all operations performed by that service when
// it disappears.
class DeviceClaimer {
 public:
  DeviceClaimer(const std::string& service_name,
                DeviceInfo* device_info,
                bool default_claimer);
  virtual ~DeviceClaimer();

  virtual bool StartServiceWatcher(
      ControlInterface* control_interface,
      const base::Closure& connection_vanished_callback);

  virtual bool Claim(const std::string& device_name, Error* error);
  virtual bool Release(const std::string& device_name, Error* error);

  // Return true if there are devices claimed by this claimer, false
  // otherwise.
  virtual bool DevicesClaimed();

  // Return true if the specified device is released by this claimer, false
  // otherwise.
  virtual bool IsDeviceReleased(const std::string& device_name);

  const std::string& name() const { return service_name_; }

  virtual bool default_claimer() const { return default_claimer_; }

  const std::set<std::string>& claimed_device_names() const {
    return claimed_device_names_;
  }

 private:
  // Watcher for monitoring the remote RPC service of the claimer.
  std::unique_ptr<RPCServiceWatcherInterface> service_watcher_;
  // The name of devices that have been claimed by this claimer.
  std::set<std::string> claimed_device_names_;
  // The name of devices that have been released by this claimer.
  std::set<std::string> released_device_names_;
  // Service name of the claimer.
  std::string service_name_;

  DeviceInfo* device_info_;

  // Flag indicating if this is the default claimer. When set to true, this
  // claimer will only be deleted when shill terminates.
  bool default_claimer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceClaimer);
};

}  // namespace shill

#endif  // SHILL_DEVICE_CLAIMER_H_

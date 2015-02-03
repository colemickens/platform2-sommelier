// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_CLOUD_DELEGATE_H_
#define PRIVETD_CLOUD_DELEGATE_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/memory/ref_counted.h>

#include "privetd/privet_types.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace privetd {

class DeviceDelegate;

// Interface to provide GCD functionality for PrivetHandler.
class CloudDelegate {
 public:
  CloudDelegate();
  virtual ~CloudDelegate();

  // Returns status of the GCD connection.
  virtual ConnectionState GetConnectionState() const = 0;

  // Returns status of the last setup.
  virtual SetupState GetSetupState() const = 0;

  // Starts GCD setup.
  virtual bool Setup(const std::string& ticket_id, const std::string& user) = 0;

  // Returns cloud id if the registered device or empty string if unregistered.
  virtual std::string GetCloudId() const = 0;

  // Create default instance.
  static std::unique_ptr<CloudDelegate> CreateDefault(
      const scoped_refptr<dbus::Bus>& bus,
      DeviceDelegate* device,
      // Allows owner to know that state of the object was changed. Used to
      // notify PeerdClient.
      const base::Closure& on_changed);
};

}  // namespace privetd

#endif  // PRIVETD_CLOUD_DELEGATE_H_

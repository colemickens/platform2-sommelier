// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_CLOUD_DELEGATE_H_
#define PRIVETD_CLOUD_DELEGATE_H_

#include <string>

namespace privetd {

enum class CloudState {
  kConnecting,
  kOnline,
  kOffline,
  kUnconfigured,
  kDisabled,
};

enum class RegistrationState {
  kAvalible,
  kCompleted,
  kInProgress,
  kInvalidTicket,
  kServerError,
  kOffline,
  kDeviceConfigError,
};

// Interface to provide GCD functionality for PrivetHandler.
class CloudDelegate {
 public:
  CloudDelegate();
  virtual ~CloudDelegate();

  // Returns true if GCD registration is required.
  virtual bool IsRegistrationRequired() const = 0;

  // Returns cloud id if the registered device or empty string if unregistered.
  virtual std::string GetCloudId() const = 0;

  // Returns state of the GCD connection of the device.
  virtual CloudState GetConnectionState() const = 0;

  // Returns registration state of the device.
  virtual RegistrationState GetRegistrationState() const = 0;

  // Starts device registration in GCD.
  virtual bool RegisterDevice(const std::string& ticket_id,
                              const std::string& user) = 0;
};

}  // namespace privetd

#endif  // PRIVETD_CLOUD_DELEGATE_H_

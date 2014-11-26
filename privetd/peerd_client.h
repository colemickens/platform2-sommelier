// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRIVETD_PEERD_CLIENT_H_
#define PRIVETD_PEERD_CLIENT_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/memory/ref_counted.h>

#include "peerd/dbus-proxies.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace privetd {

class CloudDelegate;
class DeviceDelegate;

// Publishes prived service on mDns using peerd.
class PeerdClient {
 public:
  PeerdClient(const scoped_refptr<dbus::Bus>& bus,
              const DeviceDelegate& device,
              const CloudDelegate* cloud);
  ~PeerdClient();

  // Starts publishing.
  void Start();

  // Stops publishing.
  void Stop();

 private:
  org::chromium::peerd::ManagerProxy manager_proxy_;

  std::string service_token_;

  const DeviceDelegate& device_;
  const CloudDelegate* cloud_{nullptr};  // Can be nullptr.
};

}  // namespace privetd

#endif  // PRIVETD_PEERD_CLIENT_H_

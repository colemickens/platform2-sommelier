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
class WifiDelegate;

// Publishes prived service on mDns using peerd.
class PeerdClient {
 public:
  PeerdClient(const scoped_refptr<dbus::Bus>& bus,
              const DeviceDelegate* device,
              const CloudDelegate* cloud,
              const WifiDelegate* wifi);
  ~PeerdClient();

  // Updates published information.  Removes service if HTTP is not alive.
  void Update();

 private:
  void OnPeerdOnline(org::chromium::peerd::ManagerProxy* manager_proxy);
  void OnPeerdOffline(const dbus::ObjectPath& object_path);

  void Start();
  void Stop();

  void RestartImpl();

  org::chromium::peerd::ObjectManagerProxy peerd_object_manager_proxy_;
  // |peerd_manager_proxy_| is owned by |peerd_object_manager_proxy_|.
  org::chromium::peerd::ManagerProxy* peerd_manager_proxy_{nullptr};

  std::string service_token_;

  const DeviceDelegate* device_{nullptr};  // Can't be nullptr.
  const CloudDelegate* cloud_{nullptr};  // Can be nullptr.
  const WifiDelegate* wifi_{nullptr};      // Can be nullptr.

  base::WeakPtrFactory<PeerdClient> restart_weak_ptr_factory_{this};
  base::WeakPtrFactory<PeerdClient> weak_ptr_factory_{this};
};

}  // namespace privetd

#endif  // PRIVETD_PEERD_CLIENT_H_

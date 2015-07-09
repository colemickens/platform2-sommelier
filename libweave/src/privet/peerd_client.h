// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBWEAVE_SRC_PRIVET_PEERD_CLIENT_H_
#define LIBWEAVE_SRC_PRIVET_PEERD_CLIENT_H_

#include <memory>
#include <string>

#include <base/callback.h>
#include <base/memory/ref_counted.h>

#include "libweave/src/privet/identity_delegate.h"
#include "peerd/dbus-proxies.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace weave {
namespace privet {

class CloudDelegate;
class DeviceDelegate;
class WifiDelegate;

// Publishes prived service on mDns using peerd.
class PeerdClient : public IdentityDelegate {
 public:
  PeerdClient(const scoped_refptr<dbus::Bus>& bus,
              const DeviceDelegate* device,
              const CloudDelegate* cloud,
              const WifiDelegate* wifi);
  ~PeerdClient();

  // Get the unique identifier for this device.  Note that if peerd has
  // never been seen, this may be the empty string.
  std::string GetId() const override;

  // Updates published information.  Removes service if HTTP is not alive.
  void Update();

 private:
  void OnPeerdOnline(org::chromium::peerd::ManagerProxy* manager_proxy);
  void OnPeerdOffline(const dbus::ObjectPath& object_path);
  void OnNewPeer(org::chromium::peerd::PeerProxy* peer_proxy);
  void OnPeerPropertyChanged(org::chromium::peerd::PeerProxy* peer_proxy,
                             const std::string& property_name);

  void ExposeService();
  void RemoveService();

  void UpdateImpl();

  org::chromium::peerd::ObjectManagerProxy peerd_object_manager_proxy_;
  // |peerd_manager_proxy_| is owned by |peerd_object_manager_proxy_|.
  org::chromium::peerd::ManagerProxy* peerd_manager_proxy_{nullptr};

  const DeviceDelegate* device_{nullptr};
  const CloudDelegate* cloud_{nullptr};
  const WifiDelegate* wifi_{nullptr};

  // Cached value of the device ID that we got from peerd.
  std::string device_id_;

  base::WeakPtrFactory<PeerdClient> restart_weak_ptr_factory_{this};
  base::WeakPtrFactory<PeerdClient> weak_ptr_factory_{this};
};

}  // namespace privet
}  // namespace weave

#endif  // LIBWEAVE_SRC_PRIVET_PEERD_CLIENT_H_

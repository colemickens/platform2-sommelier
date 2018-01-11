// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_PEER_MANAGER_IMPL_H_
#define PEERD_PEER_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <string>

#include <base/time/time.h>
#include <dbus/bus.h>

#include "peerd/discovered_peer.h"
#include "peerd/peer_manager_interface.h"
#include "peerd/service.h"
#include "peerd/technologies.h"

namespace peerd {

class PeerManagerImpl : public PeerManagerInterface {
 public:
  PeerManagerImpl(const scoped_refptr<dbus::Bus> bus,
                  brillo::dbus_utils::ExportedObjectManager* object_manager);
  ~PeerManagerImpl() override = default;

  // Inherited from PeerManagerInterface.  See comments there.
  void OnPeerDiscovered(const std::string& peer_id,
                        const base::Time& last_seen,
                        technologies::Technology technology) override;
  void OnServiceDiscovered(const std::string& peer_id,
                           const std::string& service_id,
                           const Service::ServiceInfo& info,
                           const Service::IpAddresses& addresses,
                           const base::Time& last_seen,
                           technologies::Technology technology) override;
  void OnPeerRemoved(const std::string& peer_id,
                     technologies::Technology technology) override;
  void OnServiceRemoved(const std::string& peer_id,
                        const std::string& service_id,
                        technologies::Technology technology) override;
  void OnTechnologyShutdown(technologies::Technology technology) override;

 private:
  scoped_refptr<dbus::Bus> bus_;
  brillo::dbus_utils::ExportedObjectManager* object_manager_;
  std::map<std::string, std::unique_ptr<DiscoveredPeer>> peers_;
  uint64_t peers_discovered_{0};
  DISALLOW_COPY_AND_ASSIGN(PeerManagerImpl);
};

}  // namespace peerd

#endif  // PEERD_PEER_MANAGER_IMPL_H_

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_PEER_MANAGER_INTERFACE_H_
#define PEERD_PEER_MANAGER_INTERFACE_H_

#include <string>

#include <base/time/time.h>

#include "peerd/service.h"
#include "peerd/technologies.h"

namespace peerd {

class PeerManagerInterface {
 public:
  virtual ~PeerManagerInterface() = default;
  // Add or update a peer with the given information.  If the same peer
  // is discovered on multiple technologies, the most recent information
  // (according to |last_seen|) is maintained.  We continue exposing
  // a peer until all technologies remove it.
  virtual void OnPeerDiscovered(const std::string& peer_id,
                                const base::Time& last_seen,
                                technologies::tech_t which_technology) = 0;
  // Adds or updates service for peer with |peer_id|.  Corresponding
  // peer's last time seen is set to max(|last_seen|, |peer.last_seen|).
  // If the same service is seen on multiple technologies, the most recent
  // update (according to |last_seen|) is maintained.
  virtual void OnServiceDiscovered(const std::string& peer_id,
                                   const std::string& service_id,
                                   const Service::ServiceInfo& info,
                                   const Service::IpAddresses& addresses,
                                   const base::Time& last_seen,
                                   technologies::tech_t which_technology) = 0;
  // Signals that the peer corresponding to |peer_id| is gone.
  // Removes knowledge of the peer and knowledge of services exposed by the peer
  // discovered via this technology.  Note that we continue to believe peers
  // and their services exist until all technologies agree that a peer is gone.
  virtual void OnPeerRemoved(const std::string& peer_id,
                             technologies::tech_t which_technology) = 0;
  // Signals that the service corresponding to |service_id| has been removed
  // from the peer corresponding to |peer_id| according to |which_technology|.
  // Note that we continue exposing a service until all technologies remove it.
  virtual void OnServiceRemoved(const std::string& peer_id,
                                const std::string& service_id,
                                technologies::tech_t which_technology) = 0;
  // Remove all services and peers discovered by this technology.  Equivalent
  // to calling OnPeerRemoved() for all peers discovered via |which_technology|.
  virtual void OnTechnologyShutdown(technologies::tech_t which_technology) = 0;
};

}  // namespace peerd

#endif  // PEERD_PEER_MANAGER_INTERFACE_H_

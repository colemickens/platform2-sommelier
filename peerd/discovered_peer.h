// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_DISCOVERED_PEER_H_
#define PEERD_DISCOVERED_PEER_H_

#include <map>
#include <string>

#include <base/time/time.h>

#include "peerd/peer.h"
#include "peerd/technologies.h"

namespace peerd {

class DiscoveredPeer : public Peer {
 public:
  DiscoveredPeer(const scoped_refptr<dbus::Bus>& bus,
                 chromeos::dbus_utils::ExportedObjectManager* object_manager,
                 const dbus::ObjectPath& path,
                 technologies::tech_t which_technology);
  ~DiscoveredPeer() override = default;

  // Update |this| with the most recent time |last_seen|.  Note that if
  // |last_seen| is older than the current value we'll discard this
  // advertisement.  Remember that we've seen this peer on the given
  // |technology|.
  virtual void UpdateFromAdvertisement(const base::Time& last_seen,
                                       technologies::tech_t technology);
  // Add or update an existing service, and record that we've seen it
  // on the given |technology|.  Note that if the service has been updated
  // more recently than |last_seen|, we'll discard this update.  Remember that
  // we've seen advertisements on |technology| for the given service, and the
  // peer itself.  Updates peer |last_seen| if more recent that the last
  // update.
  virtual void UpdateService(const std::string& service_id,
                             const Service::IpAddresses& addresses,
                             const Service::ServiceInfo& info,
                             const base::Time& last_seen,
                             technologies::tech_t technology);
  // Remove knowledge that we were seen on the given technology from |this|
  // and child services.
  virtual void RemoveTechnology(technologies::tech_t technology);
  // Remove knowledge that a service was seen on |technology| from service.
  // Removes the service if no remaining technologies claim to have seen it.
  virtual void RemoveTechnologyFromService(const std::string& service_id,
                                           technologies::tech_t technology);
  // Returns the number of technologies that we've seen this peer, or child
  // services have been updated over.
  virtual size_t GetTechnologyCount() const;

 private:
  struct ServiceMetadata {
    technologies::tech_t technology;
    base::Time last_seen;
  };
  std::map<std::string, ServiceMetadata> service_metadata_;
  technologies::tech_t discovered_on_technologies_;

  friend class DiscoveredPeerTest;
  DISALLOW_COPY_AND_ASSIGN(DiscoveredPeer);
};

}  // namespace peerd

#endif  // PEERD_DISCOVERED_PEER_H_

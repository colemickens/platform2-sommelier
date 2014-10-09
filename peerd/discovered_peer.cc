// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/discovered_peer.h"

#include <bitset>
#include <limits>
#include <string>

using chromeos::dbus_utils::ExportedObjectManager;
using peerd::technologies::tech_t;
using std::bitset;
using std::string;

namespace peerd {

DiscoveredPeer::DiscoveredPeer(const scoped_refptr<dbus::Bus>& bus,
                               ExportedObjectManager* object_manager,
                               const dbus::ObjectPath& path,
                               tech_t which_technology)
    : Peer(bus, object_manager, path),
      discovered_on_technologies_(which_technology) {
}

void DiscoveredPeer::UpdateFromAdvertisement(const std::string& name,
                                             const std::string& note,
                                             const base::Time& last_seen,
                                             tech_t technology) {
  if (!IsValidFriendlyName(nullptr, name) ||
      !IsValidNote(nullptr, note) ||
      !IsValidUpdateTime(nullptr, last_seen)) {
    return;
  }
  SetFriendlyName(nullptr, name);
  SetNote(nullptr, note);
  SetLastSeen(nullptr, last_seen);
  discovered_on_technologies_ |= technology;
}

void DiscoveredPeer::UpdateService(const std::string& service_id,
                                   const Service::IpAddresses& addresses,
                                   const Service::ServiceInfo& info,
                                   const base::Time& last_seen,
                                   tech_t technology) {
  if ((discovered_on_technologies_ & technology) == 0) {
    // We're updating a service for a technology, even though we haven't found
    // this peer on that technology.  We could allow this, but lets not until
    // we know this is valid use case.
    LOG(WARNING) << "Found service=" << service_id
                 << " on technology=" << technology
                 << " but had not previously found a peer on that service.";
    return;
  }
  // Regardless of what we do with the service update, we have new information
  // about this peer, so this counts as "seeing it."
  SetLastSeen(nullptr, last_seen);  // Ignore errors.
  auto service_it = services_.find(service_id);
  if (service_it != services_.end()) {
    auto metadata_it = service_metadata_.find(service_id);
    CHECK(metadata_it != service_metadata_.end());
    if (last_seen <  metadata_it->second.last_seen) {
      LOG(WARNING) << "Discarding stale service update.";
      return;
    }
    if (!service_it->second->Update(nullptr, addresses, info)) {
      LOG(WARNING) << "Discarding invalid service update.";
      return;
    }
    metadata_it->second.technology |= technology;
    metadata_it->second.last_seen = last_seen;
    return;
  }
  // A new service is discovered!  Exposed it over DBus and update our metadata.
  if (!Peer::AddService(nullptr, service_id, addresses, info)) {
    LOG(WARNING) << "Failed to publish discovered service over DBus.";
    return;
  }
  service_metadata_[service_id] = {technology, last_seen};
}

void DiscoveredPeer::RemoveTechnology(tech_t technology) {
  discovered_on_technologies_ &= ~technology;
  auto it = service_metadata_.begin();
  while (it != service_metadata_.end()) {
    it->second.technology &= ~technology;
    if (it->second.technology == 0) {
      RemoveService(nullptr, it->first);
      it = service_metadata_.erase(it);
    } else {
      ++it;
    }
  }
}

void DiscoveredPeer::RemoveTechnologyFromService(const std::string& service_id,
                                                 tech_t technology) {
  auto it = service_metadata_.find(service_id);
  if (it == service_metadata_.end()) {
    LOG(WARNING) << "Failed to find service previously discovered over "
                 << "technology=" << technology;
    return;
  }
  it->second.technology &= ~technology;
  // Remove this service if there are no technologies claiming to see it.
  if (it->second.technology == 0) {
    service_metadata_.erase(it);
    RemoveService(nullptr, service_id);
  }
}

size_t DiscoveredPeer::GetTechnologyCount() const {
  return bitset<std::numeric_limits<tech_t>::digits>(
      discovered_on_technologies_).count();
}

}  // namespace peerd

// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/peer_manager_impl.h"

using peerd::technologies::tech_t;
using std::string;

namespace peerd {

PeerManagerImpl::PeerManagerImpl(const scoped_refptr<dbus::Bus> bus,
                  chromeos::dbus_utils::ExportedObjectManager* object_manager)
    : bus_(bus),
      object_manager_(object_manager) {
}

void PeerManagerImpl::OnPeerDiscovered(const string& peer_id,
                                       const string& name,
                                       const string& note,
                                       const base::Time& last_seen,
                                       tech_t which_technology) {
}

void PeerManagerImpl::OnServiceDiscovered(const string& peer_id,
                                          const string& service_id,
                                          const Service::ServiceInfo& info,
                                          const Service::IpAddresses& addresses,
                                          const base::Time& last_seen,
                                          tech_t which_technology) {
}

void PeerManagerImpl::OnPeerRemoved(const string& peer_id,
                                    tech_t which_technology) {
}

void PeerManagerImpl::OnServiceRemoved(const string& peer_id,
                                       const string& service_id,
                                       tech_t which_technology) {
}

void PeerManagerImpl::OnTechnologyShutdown(tech_t which_technology) {
}

}  // namespace peerd

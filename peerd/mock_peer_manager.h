// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MOCK_PEER_MANAGER_H_
#define PEERD_MOCK_PEER_MANAGER_H_

#include <string>

#include <base/time/time.h>
#include <gmock/gmock.h>

#include "peerd/peer_manager_interface.h"
#include "peerd/service.h"
#include "peerd/technologies.h"

namespace peerd {

class MockPeerManager : public PeerManagerInterface {
 public:
  MOCK_METHOD3(OnPeerDiscovered, void(const std::string& peer_id,
                                      const base::Time& last_seen,
                                      technologies::tech_t which_technology));
  MOCK_METHOD6(OnServiceDiscovered,
               void(const std::string& peer_id,
                    const std::string& service_id,
                    const Service::ServiceInfo& info,
                    const Service::IpAddresses& addresses,
                    const base::Time& last_seen,
                    technologies::tech_t which_technology));
  MOCK_METHOD2(OnPeerRemoved, void(const std::string& peer_id,
                                   technologies::tech_t which_technology));
  MOCK_METHOD3(OnServiceRemoved, void(const std::string& peer_id,
                                      const std::string& service_id,
                                      technologies::tech_t which_technology));
  MOCK_METHOD1(OnTechnologyShutdown,
               void(technologies::tech_t which_technology));
};

}  // namespace peerd

#endif  // PEERD_MOCK_PEER_MANAGER_H_

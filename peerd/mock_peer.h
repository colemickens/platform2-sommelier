// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MOCK_PEER_H_
#define PEERD_MOCK_PEER_H_

#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/errors/error.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>

#include "peerd/ip_addr.h"
#include "peerd/peer.h"
#include "peerd/service_publisher_interface.h"

namespace peerd {

class MockPeer : public Peer {
 public:
  MockPeer(const std::string& path_prefix, const std::string& uuid)
      : Peer(std::unique_ptr<chromeos::dbus_utils::DBusObject>{},
             dbus::ObjectPath{path_prefix},
             uuid) {
  }
  ~MockPeer() override = default;

  MOCK_CONST_METHOD0(GetUUID, std::string());
  MOCK_CONST_METHOD0(GetFriendlyName, std::string());
  MOCK_CONST_METHOD0(GetNote, std::string());
  MOCK_METHOD2(SetFriendlyName, bool(chromeos::ErrorPtr* error,
                                     const std::string& friendly_name));
  MOCK_METHOD2(SetNote, bool(chromeos::ErrorPtr* error,
                             const std::string& note));
  MOCK_METHOD1(SetLastSeen, void(uint64_t last_seen));

  MOCK_METHOD4(AddService,
               bool(chromeos::ErrorPtr* error,
                    const std::string& service_id,
                    const std::vector<ip_addr>& addresses,
                    const std::map<std::string, std::string>& service_info));
  MOCK_METHOD2(RemoveService, bool(chromeos::ErrorPtr* error,
                                   const std::string& service_id));
  MOCK_METHOD1(RegisterServicePublisher,
               void(base::WeakPtr<ServicePublisherInterface> publisher));
};

}  // namespace peerd

#endif  // PEERD_MOCK_PEER_H_

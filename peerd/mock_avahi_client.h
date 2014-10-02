// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_MOCK_AVAHI_CLIENT_H_
#define PEERD_MOCK_AVAHI_CLIENT_H_

#include "peerd/mock_peer_manager.h"
#include "peerd/peer.h"

#include <string>
#include <gmock/gmock.h>

namespace peerd {

class MockAvahiClient: public AvahiClient {
 public:
  MockAvahiClient(const scoped_refptr<dbus::Bus>& bus,
                  MockPeerManager* peer_manager)
      : AvahiClient(bus, peer_manager) {
    EXPECT_CALL(*peer_manager, OnTechnologyShutdown(technologies::kMDNS));
  }
  ~MockAvahiClient() override = default;
  MOCK_METHOD1(RegisterAsync, void(const CompletionAction& cb));
  MOCK_METHOD1(RegisterOnAvahiRestartCallback,
               void(const OnAvahiRestartCallback& cb));
  MOCK_METHOD3(GetPublisher,
               base::WeakPtr<ServicePublisherInterface>(
                   const std::string& uuid,
                   const std::string& friendly_name,
                   const std::string& note));
  MOCK_METHOD0(StartMonitoring, void());
  MOCK_METHOD0(StopMonitoring, void());
};

}  // namespace peerd

#endif  // PEERD_MOCK_AVAHI_CLIENT_H_

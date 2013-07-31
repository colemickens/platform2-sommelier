// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_PEER_SELECTOR_H__
#define P2P_CLIENT_PEER_SELECTOR_H__

#include "client/service_finder.h"
#include "client/clock.h"

#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST

namespace p2p {

namespace client {

// Interface for finding local peers willing to serve files.
class PeerSelector {
 public:
  // Constructs the PeerSelector with the provided interfaces.
  PeerSelector(ServiceFinder* finder, ClockInterface* clock);

  // Finds an URL for the file |id| with at least |minimum_size| bytes and
  // waits until the number of connections in the LAN has dropped below the
  // required threshold. If there are no peers sharing this file with at least
  // |minimum_size| bytes returns "" regardless of the number of connections in
  // the LAN. On success, returns the URL found.
  std::string GetUrlAndWait(const std::string& id, size_t minimum_size);

 private:
  friend class PeerSelectorTest;
  FRIEND_TEST(PeerSelectorTest, PickUrlForNonExistantId);
  FRIEND_TEST(PeerSelectorTest, PickUrlForIdWithZeroBytes);
  FRIEND_TEST(PeerSelectorTest, PickUrlForIdWithMinimumSize);
  FRIEND_TEST(PeerSelectorTest, PickUrlFromTheFirstThird);

  // file |id| with at least |minimum_size| bytes. If no peer is found meeting
  // those conditions, an empty string is returned. Otherwise, the URL of the
  // provided file is returned.
  std::string PickUrlForId(const std::string& id, size_t minimum_size);

  // The underlying service finder class used.
  ServiceFinder* finder_;

  // An interface to the system clock functions, used for unit testing.
  ClockInterface* clock_;

  DISALLOW_COPY_AND_ASSIGN(PeerSelector);
};

}  // namespace client

}  // namespace p2p

#endif  // P2P_CLIENT_PEER_SELECTOR_H__


// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client/fake_clock.h"
#include "client/fake_service_finder.h"
#include "client/peer_selector.h"
#include "common/testutil.h"

#include <string>

#include <base/bind.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace p2p {

namespace client {

class PeerSelectorTest : public ::testing::Test {
 public:
  PeerSelectorTest() : ps_(&sf_, &clock_) {}

 protected:
  FakeClock clock_;
  FakeServiceFinder sf_;
  PeerSelector ps_; // The PeerSelector under test.
};

TEST_F(PeerSelectorTest, PickUrlForNonExistantId) {
  EXPECT_EQ(ps_.PickUrlForId("non-existant"), "");

  // Share some *other* files on the network.
  int peer = sf_.NewPeer("10.0.0.1", false, 1111);
  ASSERT_TRUE(sf_.PeerShareFile(peer, "some-file", 10240));
  ASSERT_TRUE(sf_.PeerShareFile(peer, "other-file", 10240));
  EXPECT_EQ(ps_.PickUrlForId("non-existant"), "");

  // PickUrlForId should not call Lookup().
  EXPECT_EQ(sf_.GetNumLookupCalls(), 0);
}

TEST_F(PeerSelectorTest, PickUrlForIdWithZeroBytes) {
  int peer1 = sf_.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf_.NewPeer("10.0.0.2", false, 2222);
  ASSERT_TRUE(sf_.PeerShareFile(peer1, "some-file", 0));
  ASSERT_TRUE(sf_.PeerShareFile(peer2, "some-file", 0));
  // PickUrlForId() should not return an URL for a peer sharing a 0-bytes file.
  EXPECT_EQ(ps_.PickUrlForId("some-file"), "");
}

TEST_F(PeerSelectorTest, PickUrlFromTheFirstThird) {
  int peer1 = sf_.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf_.NewPeer("10.0.0.2", false, 2222);
  int peer3 = sf_.NewPeer("10.0.0.3", false, 3333);
  int peer4 = sf_.NewPeer("10.0.0.4", false, 4444);
  ASSERT_TRUE(sf_.PeerShareFile(peer1, "some-file", 1000));
  ASSERT_TRUE(sf_.PeerShareFile(peer2, "some-file", 500));
  ASSERT_TRUE(sf_.PeerShareFile(peer3, "some-file", 300));
  ASSERT_TRUE(sf_.PeerShareFile(peer4, "some-file", 0));
  EXPECT_EQ(ps_.PickUrlForId("some-file"), "http://10.0.0.1:1111/some-file");
}

TEST_F(PeerSelectorTest, GetUrlAndWaitWithNoPeers) {
  EXPECT_EQ(ps_.GetUrlAndWait("some-file"), "");

  // GetUrlAndWait() should only call Lookup() if it needs to wait.
  EXPECT_EQ(sf_.GetNumLookupCalls(), 0);
}

TEST_F(PeerSelectorTest, GetUrlAndWaitWithUnknownFile) {
  int peer1 = sf_.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf_.NewPeer("10.0.0.2", false, 2222);
  ASSERT_TRUE(sf_.PeerShareFile(peer1, "some-file", 1000));
  ASSERT_TRUE(sf_.PeerShareFile(peer2, "some-file", 500));

  EXPECT_EQ(ps_.GetUrlAndWait("unknown-file"), "");

  // GetUrlAndWait() should only call Lookup() if it needs to wait.
  EXPECT_EQ(sf_.GetNumLookupCalls(), 0);
}

TEST_F(PeerSelectorTest, GetUrlAndWaitOnBusyNetwork) {
  // This test checks that GetUrlAndWait() doesn't return an URL for a file if
  // there are already too many connections on the network. The current limit is
  // set to 3. Update this test if you itentionally changed that value.
  const int max_connections = 3;

  int peer1 = sf_.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf_.NewPeer("10.0.0.2", false, 2222);
  ASSERT_TRUE(sf_.PeerShareFile(peer1, "some-file", 1000));
  ASSERT_TRUE(sf_.PeerShareFile(peer2, "some-file", 500));
  ASSERT_TRUE(sf_.SetPeerConnections(peer1, max_connections));
  ASSERT_TRUE(sf_.SetPeerConnections(peer2, max_connections-1));

  // After 2 Lookup() calls, the network is not as busy (|max_connections|
  // connections), but still not enough.
  ASSERT_TRUE(sf_.SetPeerConnectionsOnLookup(2, peer2, 0));

  // After 4 Lookup() calls, the network reaches the limit to allow the
  // download.
  ASSERT_TRUE(sf_.SetPeerConnectionsOnLookup(4, peer1, max_connections-1));

  // Make the test finish if more than 10 Lookup()'s are made.
  ASSERT_TRUE(sf_.RemoveAvailableFileOnLookup(10, "some-file"));

  // GetUrlAndWait should return the biggest file in this case.
  EXPECT_EQ(ps_.GetUrlAndWait("some-file"), "http://10.0.0.1:1111/some-file");

  EXPECT_EQ(sf_.GetNumLookupCalls(), 4);
  EXPECT_EQ(clock_.GetSleptTime(), 4 * 30);
}

TEST_F(PeerSelectorTest, GetUrlAndWaitWhenThePeerGoesAway) {
  int peer1 = sf_.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf_.NewPeer("10.0.0.2", false, 2222);
  ASSERT_TRUE(sf_.PeerShareFile(peer1, "some-file", 1000));
  ASSERT_TRUE(sf_.PeerShareFile(peer2, "some-file", 500));
  ASSERT_TRUE(sf_.PeerShareFile(peer2, "other-file", 500));
  // A super-busy network.
  ASSERT_TRUE(sf_.SetPeerConnections(peer2, 999));

  // After 3 Lookup()'s, peer2 lost the file.
  ASSERT_TRUE(sf_.RemoveAvailableFileOnLookup(3, "some-file"));
  ASSERT_TRUE(sf_.PeerShareFileOnLookup(3, peer1, "some-file", 1000));

  // After 5 Lookup()'s, network is still busy, but the file is not present
  // anymore.
  ASSERT_TRUE(sf_.RemoveAvailableFileOnLookup(5, "some-file"));

  // To ensure test completion (with failure) remove any other file after 10
  // Lookup()'s.
  ASSERT_TRUE(sf_.SetPeerConnectionsOnLookup(10, peer2, 0));
  ASSERT_TRUE(sf_.RemoveAvailableFileOnLookup(10, "other-file"));

  EXPECT_EQ(ps_.GetUrlAndWait("some-file"), "");

  EXPECT_EQ(sf_.GetNumLookupCalls(), 5);
  EXPECT_EQ(clock_.GetSleptTime(), 5 * 30);
}

}  // namespace client

}  // namespace p2p

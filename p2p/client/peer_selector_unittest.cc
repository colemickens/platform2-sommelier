
// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "client/peer_selector.h"
#include "client/fake_service_finder.h"
#include "common/testutil.h"

#include <string>

#include <base/bind.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace p2p {

namespace client {

TEST(PeerSelector, PickUrlForNonExistantId) {
  FakeServiceFinder sf;

  EXPECT_EQ(PickUrlForId(&sf, "non-existant"), "");

  // Share some *other* files on the network.
  int peer = sf.NewPeer("10.0.0.1", false, 1111);
  ASSERT_TRUE(sf.PeerShareFile(peer, "some-file", 10240));
  ASSERT_TRUE(sf.PeerShareFile(peer, "other-file", 10240));
  EXPECT_EQ(PickUrlForId(&sf, "non-existant"), "");
}

TEST(PeerSelector, PickUrlForIdWithZeroBytes) {
  FakeServiceFinder sf;

  int peer1 = sf.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf.NewPeer("10.0.0.2", false, 2222);
  ASSERT_TRUE(sf.PeerShareFile(peer1, "some-file", 0));
  ASSERT_TRUE(sf.PeerShareFile(peer2, "some-file", 0));
  // PickUrlForId() should not return an URL for a peer sharing a 0-bytes file.
  EXPECT_EQ(PickUrlForId(&sf, "some-file"), "");
}

TEST(PeerSelector, PickUrlFromTheFirstThird) {
  FakeServiceFinder sf;

  int peer1 = sf.NewPeer("10.0.0.1", false, 1111);
  int peer2 = sf.NewPeer("10.0.0.2", false, 2222);
  int peer3 = sf.NewPeer("10.0.0.3", false, 3333);
  int peer4 = sf.NewPeer("10.0.0.4", false, 4444);
  ASSERT_TRUE(sf.PeerShareFile(peer1, "some-file", 1000));
  ASSERT_TRUE(sf.PeerShareFile(peer2, "some-file", 500));
  ASSERT_TRUE(sf.PeerShareFile(peer3, "some-file", 300));
  ASSERT_TRUE(sf.PeerShareFile(peer4, "some-file", 0));
  EXPECT_EQ(PickUrlForId(&sf, "some-file"), "http://10.0.0.1:1111/some-file");
}

}  // namespace client

}  // namespace p2p

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_FAKE_SERVICE_FINDER_H__
#define P2P_CLIENT_FAKE_SERVICE_FINDER_H__

#include "client/service_finder.h"

#include <map>
#include <string>

namespace p2p {

namespace client {

class FakeServiceFinder : public ServiceFinder {
 public:
  FakeServiceFinder();
  virtual ~FakeServiceFinder();

  std::vector<const Peer*> GetPeersForFile(const std::string& file) const;

  std::vector<std::string> AvailableFiles() const;

  int NumTotalConnections() const;

  void Lookup();

  // NewPeer() creates a new Peer object with the given properties. The return
  // value is a peer_id used only in the context of the fake implementation.
  int NewPeer(std::string address, bool is_ipv6, uint16 port);

  // SetPeerConnections() sets the number of active connections reported by a
  // given peer. The |peer_id| argument is the numeric peer id returned by
  // NewPeer().
  bool SetPeerConnections(int peer_id, int connections);

  // PeerShareFile() will make the peer referred by |peer_id| to share the file
  // |file| with the current size |size|. If the file was previously shared by
  // that peer, the file size will be updated.
  // In case of an error, this method returns false. Otherwise returns true.
  bool PeerShareFile(int peer_id, const std::string& file, size_t size);

  // RemoveAvailableFile() removes a previously added file |file|. All the peers
  // sharing the given fill will drop it. If the file wasn't previously added
  // this method returns false. Otherwise, it returns true.
  bool RemoveAvailableFile(const std::string& file);

 private:
  // The list of peers on the network.
  std::vector<Peer> peers_;

  DISALLOW_COPY_AND_ASSIGN(FakeServiceFinder);
};

}  // namespace client

}  // namespace p2p

#endif  // P2P_CLIENT_FAKE_SERVICE_FINDER_H__

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client/fake_service_finder.h"

#include <base/logging.h>

#include <set>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace p2p {

namespace client {

FakeServiceFinder::FakeServiceFinder() {
}

FakeServiceFinder::~FakeServiceFinder() {
}

vector<const Peer*> FakeServiceFinder::GetPeersForFile(
    const string& file) const {
  vector<const Peer*> res;
  for(auto const& peer : peers_) {
    if (peer.files.find(file) != peer.files.end())
      res.push_back(&peer);
  }
  return res;
}

vector<string> FakeServiceFinder::AvailableFiles() const {
  set<string> retset;
  for(auto const& peer : peers_) {
    for(auto const& file : peer.files) {
      retset.insert(file.first);
    }
  }
  return vector<string>(retset.begin(), retset.end());
}

int FakeServiceFinder::NumTotalConnections() const {
  int res = 0;
  for(auto const& peer : peers_)
    res += peer.num_connections;
  return res;
}

void FakeServiceFinder::Lookup() {
}

int FakeServiceFinder::NewPeer(string address, bool is_ipv6, uint16 port) {
  peers_.push_back((Peer){
      .address = address,
      .is_ipv6 = is_ipv6,
      .port = port,
      .num_connections = 0,
      .files = map<string, size_t>()});
  return peers_.size() - 1;
}

bool FakeServiceFinder::SetPeerConnections(int peer_id, int  connections) {
  if (peer_id < 0 || static_cast<unsigned>(peer_id) >= peers_.size()) {
    LOG(ERROR) << "Invalid peer_id provided: " << peer_id << ".";
    return false;
  }
  peers_[peer_id].num_connections = connections;
  return true;
}

bool FakeServiceFinder::PeerShareFile(int peer_id,
                                      const string& file,
                                      size_t size) {
  if (peer_id < 0 || static_cast<unsigned>(peer_id) >= peers_.size()) {
    LOG(ERROR) << "Invalid peer_id provided: " << peer_id << ".";
    return false;
  }
  peers_[peer_id].files[file] = size;
  return true;
}

bool FakeServiceFinder::RemoveAvailableFile(const string& file) {
  int removed = 0;

  for(auto& peer : peers_) {
    map<string, size_t>::iterator file_it = peer.files.find(file);
    if (file_it != peer.files.end()) {
      peer.files.erase(file_it);
      removed++;
    }
  }

  if (!removed) {
    LOG(ERROR) << "Removing unexisting  file <" << file << ">.";
    return false;
  }
  return true;
}

}  // namespace client

}  // namespace p2p

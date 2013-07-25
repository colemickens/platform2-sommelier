// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_SERVICE_FINDER_H__
#define P2P_CLIENT_SERVICE_FINDER_H__

#include "client/peer.h"

#include <vector>
#include <set>
#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/callback.h>

namespace p2p {

namespace client {

// Interface for finding local peers willing to serve files.
class ServiceFinder {
 public:
  virtual ~ServiceFinder() {}

  // Given a file identified by the |file| paramater, returns a list
  // of peers that can serve it.
  //
  // This should only be called after calling Lookup(). Does no I/O.
  virtual std::vector<const Peer*> GetPeersForFile(
      const std::string& file) const = 0;

  // Gets a list of available files served by peers on the network.
  //
  // This should only be called after calling Lookup(). Does no I/O.
  virtual std::vector<std::string> AvailableFiles() const = 0;

  // Gets the total number of p2p downloads on the local network. This
  // is defined as the sum of the "num-connections" TXT entries for
  // all _cros_p2p._tcp instances.
  //
  // This should only be called after calling Lookup(). Does no I/O.
  virtual int NumTotalConnections() const = 0;

  // Looks up services on the local network. This method does blocking
  // I/O and it can take many seconds before it returns. May be called
  // multiple times to refresh the results.
  virtual void Lookup() = 0;

  // Constructs a suitable implementation of ServiceFinder and
  // initializes it. This does blocking I/O. Returns NULL if
  // an error occured.
  static ServiceFinder* Construct();
};

}  // namespace client

}  // namespace p2p

#endif  // P2P_CLIENT_SERVICE_FINDER_H__

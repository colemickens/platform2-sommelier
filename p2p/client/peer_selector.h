// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_PEER_SELECTOR_H__
#define P2P_CLIENT_PEER_SELECTOR_H__

#include "client/service_finder.h"

#include <string>

namespace p2p {

namespace client {

// PickUrlForId() chooses a peer from the ServiceFinder |finder| sharing the
// file |id|. If no peer is found an empty string is returned. Otherwise, the
// URL of the provided file is returned.
std::string PickUrlForId(ServiceFinder* finder, const std::string& id);

// Finds a URL using |id| and waits until the number of connections in
// the LAN has dropped below the required threshold. Returns "" if the
// URL could not be found.
std::string GetUrlAndWait(ServiceFinder* finder, const std::string& id);

}  // namespace client

}  // namespace p2p

#endif  // P2P_CLIENT_PEER_SELECTOR_H__

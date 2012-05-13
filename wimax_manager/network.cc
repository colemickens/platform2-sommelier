// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wimax_manager/network.h"

using std::string;

namespace wimax_manager {

Network::Network(uint32 id, const string &name) : id_(id), name_(name) {
}

Network::~Network() {
}

}  // namespace wimax_manager

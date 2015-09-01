// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/upstart/upstart_proxy_stub.h"

using std::string;
using std::vector;

namespace shill {

UpstartProxyStub::UpstartProxyStub() {}

void UpstartProxyStub::EmitEvent(
    const string& /*name*/, const vector<string>& /*env*/, bool /*wait*/) {
  // STUB IMPLEMENTATION.
}

}  // namespace shill

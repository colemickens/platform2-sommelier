// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/shill_stub_proxy.h"

using std::string;

namespace apmanager {

ShillStubProxy::ShillStubProxy() {}

ShillStubProxy::~ShillStubProxy() {}

bool ShillStubProxy::ClaimInterface(const string& interface_name) {
  return true;
}

bool ShillStubProxy::ReleaseInterface(const string& interface_name) {
  return true;
}

}  // namespace apmanager

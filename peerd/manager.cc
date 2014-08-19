// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <string>

#include "peerd/dbus_constants.h"

using peerd::dbus_constants::kPingResponse;
using std::string;

namespace peerd {

Manager::Manager(const scoped_refptr<dbus::Bus>& bus) : proxy_(bus, this) {
}

void Manager::Init(const OnInitFinish& success_cb) {
  // Since we have no async init of our own, we can just give our
  // callback to the proxy.
  proxy_.Init(success_cb);
}

string Manager::Ping() {
  return kPingResponse;
}

}  // namespace peerd

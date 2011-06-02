// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"

#include <base/logging.h>

namespace shill {

DHCPProvider::DHCPProvider() {
  VLOG(2) << "DHCPProvider created.";
}

DHCPProvider::~DHCPProvider() {
  VLOG(2) << "DHCPProvider destroyed.";
}

DHCPProvider* DHCPProvider::GetInstance() {
  return Singleton<DHCPProvider>::get();
}

}  // namespace shill

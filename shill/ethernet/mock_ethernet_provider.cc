// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/ethernet/mock_ethernet_provider.h"

namespace shill {

MockEthernetProvider::MockEthernetProvider()
    : EthernetProvider(nullptr, nullptr, nullptr, nullptr) {}

MockEthernetProvider::~MockEthernetProvider() {}

}  // namespace shill

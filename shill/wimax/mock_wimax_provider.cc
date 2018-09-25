// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wimax/mock_wimax_provider.h"

#include "shill/wimax/wimax.h"

namespace shill {

MockWiMaxProvider::MockWiMaxProvider()
    : WiMaxProvider(nullptr, nullptr, nullptr, nullptr) {}

MockWiMaxProvider::~MockWiMaxProvider() {}

}  // namespace shill

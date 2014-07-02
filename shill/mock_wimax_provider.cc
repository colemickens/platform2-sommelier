// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_wimax_provider.h"

#include "shill/wimax.h"

namespace shill {

MockWiMaxProvider::MockWiMaxProvider()
    : WiMaxProvider(NULL, NULL, NULL, NULL) {}

MockWiMaxProvider::~MockWiMaxProvider() {}

}  // namespace shill

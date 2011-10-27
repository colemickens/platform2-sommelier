// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/cellular_capability.h"

#include "shill/cellular.h"

namespace shill {

CellularCapability::CellularCapability(Cellular *cellular)
    : cellular_(cellular),
      proxy_factory_(cellular->proxy_factory()) {}

CellularCapability::~CellularCapability() {}

}  // namespace shill

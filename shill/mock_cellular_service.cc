// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_cellular_service.h"

namespace shill {

MockCellularService::MockCellularService(ModemInfo *modem_info,
                                         const CellularRefPtr &device)
    : CellularService(modem_info, device) {}

MockCellularService::~MockCellularService() {}

}  // namespace shill

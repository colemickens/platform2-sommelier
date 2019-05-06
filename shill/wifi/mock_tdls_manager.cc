// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/mock_tdls_manager.h"

namespace shill {

MockTDLSManager::MockTDLSManager() : TDLSManager(nullptr, nullptr, "") {}

MockTDLSManager::~MockTDLSManager() = default;

}  // namespace shill

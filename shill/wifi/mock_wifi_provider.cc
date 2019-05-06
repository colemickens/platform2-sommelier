// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/wifi/mock_wifi_provider.h"

#include "shill/wifi/wifi_service.h"  // Needed for mock method instantiation.

using testing::Return;

namespace shill {

MockWiFiProvider::MockWiFiProvider()
    : WiFiProvider(nullptr, nullptr, nullptr, nullptr) {
  ON_CALL(*this, GetHiddenSSIDList()).WillByDefault(Return(ByteArrays()));
}

MockWiFiProvider::~MockWiFiProvider() = default;

}  // namespace shill

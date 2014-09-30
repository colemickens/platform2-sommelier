// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_ethernet_service.h"

#include "shill/ethernet.h"  // Needed to pass an EthernetRefPtr.

namespace shill {

MockEthernetService::MockEthernetService(ControlInterface *control_interface,
                                         Metrics *metrics)
    : EthernetService(control_interface, nullptr, metrics, nullptr,
                      EthernetRefPtr()) {}

MockEthernetService::~MockEthernetService() {}

}  // namespace shill

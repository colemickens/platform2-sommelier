// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_modem.h"

namespace shill {

MockModem::MockModem(const std::string &owner,
                     const std::string &path,
                     ControlInterface *control_interface,
                     EventDispatcher *dispatcher,
                     Metrics *metrics,
                     Manager *manager,
                     mobile_provider_db *provider_db)
    : Modem(owner, path, control_interface, dispatcher, metrics, manager,
            provider_db) {}

MockModem::~MockModem() {}

}  // namespace shill

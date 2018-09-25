// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_connectivity_trial.h"

#include "shill/connection.h"

namespace shill {

MockConnectivityTrial::MockConnectivityTrial(ConnectionRefPtr connection,
                                             int trial_timeout_seconds)
    : ConnectivityTrial(connection,
                        nullptr,
                        trial_timeout_seconds,
                        base::Callback<void(ConnectivityTrial::Result)>()) {}

MockConnectivityTrial::~MockConnectivityTrial() {}

}  // namespace shill

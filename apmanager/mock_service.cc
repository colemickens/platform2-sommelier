// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_service.h"

namespace apmanager {

MockService::MockService() : Service(nullptr, 0) {}

MockService::~MockService() {}

}  // namespace apmanager

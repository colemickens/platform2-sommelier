// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apmanager/mock_config.h"

namespace apmanager {

MockConfig::MockConfig(Manager* manager) : Config(manager, 0) {}

MockConfig::~MockConfig() {}

}  // namespace apmanager

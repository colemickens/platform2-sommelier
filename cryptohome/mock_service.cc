// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_service.h"

namespace cryptohome {

MockService::MockService(const std::string& abe_data) :
  ServiceMonolithic(abe_data) {}
MockService::MockService() : MockService(std::string()) {}
MockService::~MockService() {}

}  // namespace cryptohome

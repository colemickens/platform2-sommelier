// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_lockbox.h"

namespace cryptohome {

MockLockbox::MockLockbox() : Lockbox(NULL, 0) {}
MockLockbox::~MockLockbox() {}

}  // namespace cryptohome

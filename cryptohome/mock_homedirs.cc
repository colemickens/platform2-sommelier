// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_homedirs.h"

using testing::Return;

namespace cryptohome {

MockHomeDirs::MockHomeDirs() {
  ON_CALL(*this, Init()).WillByDefault(Return(true));
}
MockHomeDirs::~MockHomeDirs() {}

}  // namespace cryptohome

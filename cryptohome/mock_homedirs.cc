// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/mock_homedirs.h"

using testing::_;
using testing::Return;

namespace cryptohome {

MockHomeDirs::MockHomeDirs() {
  ON_CALL(*this, Init(_, _, _)).WillByDefault(Return(true));
}
MockHomeDirs::~MockHomeDirs() {}

bool MockHomeDirs::ActualGetPlainOwner(std::string* owner) {
  return HomeDirs::GetPlainOwner(owner);
}

}  // namespace cryptohome

// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef U2FD_MOCK_USER_STATE_H_
#define U2FD_MOCK_USER_STATE_H_

#include "u2fd/user_state.h"

#include <vector>

#include <base/optional.h>
#include <brillo/secure_blob.h>
#include <gmock/gmock.h>

namespace u2f {

class MockUserState : public UserState {
 public:
  MOCK_METHOD0(GetUserSecret, base::Optional<brillo::SecureBlob>(void));
  MOCK_METHOD0(GetCounter, base::Optional<std::vector<uint8_t>>(void));
  MOCK_METHOD0(IncrementCounter, bool(void));
};

}  // namespace u2f

#endif  // U2FD_MOCK_USER_STATE_H_

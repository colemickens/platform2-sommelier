// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/handle_generator.h"

namespace chaps {

class HandleGeneratorMock : public HandleGenerator {
 public:
  MOCK_METHOD0(CreateHandle, int());
};

}  // namespace chaps

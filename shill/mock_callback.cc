// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_callback.h"

namespace shill {

MockCallback::MockCallback() :
    CallbackWithReturnValueImpl<
  EmptyClass, bool (EmptyClass::*)(), bool>(NULL, NULL) {}

MockCallback::~MockCallback() {}

}  // namespace shill

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps_proxy_mock.h"

namespace chaps {

ChapsProxyMock::ChapsProxyMock(bool is_initialized) {
  EnableMockProxy(this, is_initialized);
}
ChapsProxyMock::~ChapsProxyMock() {
  DisableMockProxy();
}

}

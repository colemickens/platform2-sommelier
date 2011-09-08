// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_PROXY_MOCK_H
#define CHAPS_PROXY_MOCK_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/chaps_proxy_interface.h"

namespace chaps {

// defined in chaps.cc
extern void EnableMockProxy(ChapsProxyInterface* proxy, bool is_initialized);
extern void DisableMockProxy();

// ChapsProxyMock is a mock of ChapsProxyInterface
class ChapsProxyMock : public ChapsProxyInterface {
public:
  ChapsProxyMock(bool is_initialized) {
    EnableMockProxy(this, is_initialized);
  }
  virtual ~ChapsProxyMock() {
    DisableMockProxy();
  }

  MOCK_METHOD1(Connect, bool (const std::string&));
  MOCK_METHOD0(Disconnect, void());

private:
  DISALLOW_COPY_AND_ASSIGN(ChapsProxyMock);
};

}  // namespace

#endif  // CHAPS_PROXY_MOCK_H


// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_PROXY_MOCK_H
#define CHAPS_CHAPS_PROXY_MOCK_H

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/chaps_interface.h"

namespace chaps {

// defined in chaps.cc
extern void EnableMockProxy(ChapsInterface* proxy, bool is_initialized);
extern void DisableMockProxy();

// ChapsProxyMock is a mock of ChapsInterface
class ChapsProxyMock : public ChapsInterface {
public:
  ChapsProxyMock(bool is_initialized) {
    EnableMockProxy(this, is_initialized);
  }
  virtual ~ChapsProxyMock() {
    DisableMockProxy();
  }

  MOCK_METHOD2(GetSlotList, uint32_t (bool, std::vector<uint32_t>*));
  MOCK_METHOD8(GetSlotInfo, uint32_t (uint32_t,
                                      std::string*,
                                      std::string*,
                                      uint32_t*,
                                      uint8_t*, uint8_t*,
                                      uint8_t*, uint8_t*));

private:
  DISALLOW_COPY_AND_ASSIGN(ChapsProxyMock);
};

}  // namespace

#endif  // CHAPS_CHAPS_PROXY_MOCK_H

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
  virtual uint32_t GetTokenInfo(uint32_t slot_id,
                                std::string* label,
                                std::string* manufacturer_id,
                                std::string* model,
                                std::string* serial_number,
                                uint32_t* flags,
                                uint32_t* max_session_count,
                                uint32_t* session_count,
                                uint32_t* max_session_count_rw,
                                uint32_t* session_count_rw,
                                uint32_t* max_pin_len,
                                uint32_t* min_pin_len,
                                uint32_t* total_public_memory,
                                uint32_t* free_public_memory,
                                uint32_t* total_private_memory,
                                uint32_t* free_private_memory,
                                uint8_t* hardware_version_major,
                                uint8_t* hardware_version_minor,
                                uint8_t* firmware_version_major,
                                uint8_t* firmware_version_minor) {
    *flags = 1;
    return 0;
  }
  MOCK_METHOD2(GetMechanismList, uint32_t (uint32_t, std::vector<uint32_t>*));
  MOCK_METHOD5(GetMechanismInfo, uint32_t (uint32_t, uint32_t, uint32_t*,
                                           uint32_t*, uint32_t*));
  MOCK_METHOD3(InitToken, uint32_t (uint32_t, const std::string*,
                                    const std::string&));
  MOCK_METHOD2(InitPIN, uint32_t (uint32_t, const std::string*));
  MOCK_METHOD3(SetPIN, uint32_t (uint32_t, const std::string*,
                                 const std::string*));
  MOCK_METHOD3(OpenSession, uint32_t (uint32_t, uint32_t, uint32_t*));
  MOCK_METHOD1(CloseSession, uint32_t (uint32_t));
  MOCK_METHOD1(CloseAllSessions, uint32_t (uint32_t));

private:
  DISALLOW_COPY_AND_ASSIGN(ChapsProxyMock);
};

}  // namespace

#endif  // CHAPS_CHAPS_PROXY_MOCK_H

// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SLOT_MANAGER_MOCK_H
#define CHAPS_SLOT_MANAGER_MOCK_H

#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "pkcs11/cryptoki.h"
#include "slot_manager.h"

namespace chaps {

class SlotManagerMock : public SlotManager {
 public:
  SlotManagerMock() {}
  MOCK_CONST_METHOD0(GetSlotCount, int ());
  MOCK_METHOD0(AddSlot, int ());
  MOCK_CONST_METHOD1(IsTokenPresent, bool (int));
  MOCK_CONST_METHOD2(GetSlotInfo, void (int, CK_SLOT_INFO*));
  MOCK_CONST_METHOD2(GetTokenInfo, void (int, CK_TOKEN_INFO*));
  MOCK_CONST_METHOD1(GetMechanismInfo, MechanismMap* (int));
  MOCK_METHOD2(OpenSession, int (int, bool));
  MOCK_METHOD1(CloseSession, bool (int));
  MOCK_METHOD1(CloseAllSessions, void (int));
  MOCK_CONST_METHOD2(GetSession, bool (int, Session**));

 private:
  DISALLOW_COPY_AND_ASSIGN(SlotManagerMock);
};

}  // namespace chaps

#endif  // CHAPS_SLOT_MANAGER_MOCK_H

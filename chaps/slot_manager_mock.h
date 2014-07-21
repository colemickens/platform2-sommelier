// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SLOT_MANAGER_MOCK_H_
#define CHAPS_SLOT_MANAGER_MOCK_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chaps/slot_manager.h"
#include "pkcs11/cryptoki.h"

namespace chaps {

class SlotManagerMock : public SlotManager {
 public:
  SlotManagerMock();
  virtual ~SlotManagerMock();

  MOCK_CONST_METHOD0(GetSlotCount, int());
  MOCK_CONST_METHOD2(IsTokenPresent, bool(const chromeos::SecureBlob&, int));
  MOCK_CONST_METHOD2(IsTokenAccessible, bool(const chromeos::SecureBlob&,
                                             int));
  MOCK_CONST_METHOD3(GetSlotInfo, void(const chromeos::SecureBlob&, int,
                                       CK_SLOT_INFO*));
  MOCK_CONST_METHOD3(GetTokenInfo, void(const chromeos::SecureBlob&, int,
                                        CK_TOKEN_INFO*));
  MOCK_CONST_METHOD2(GetMechanismInfo, MechanismMap* (
      const chromeos::SecureBlob&, int));
  MOCK_METHOD3(OpenSession, int(const chromeos::SecureBlob&, int, bool));
  MOCK_METHOD2(CloseSession, bool(const chromeos::SecureBlob&, int));
  MOCK_METHOD2(CloseAllSessions, void(const chromeos::SecureBlob&, int));
  MOCK_CONST_METHOD3(GetSession, bool(const chromeos::SecureBlob&, int,
                                      Session**));

 private:
  DISALLOW_COPY_AND_ASSIGN(SlotManagerMock);
};

}  // namespace chaps

#endif  // CHAPS_SLOT_MANAGER_MOCK_H_

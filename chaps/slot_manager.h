// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SLOT_MANAGER_H
#define CHAPS_SLOT_MANAGER_H

#include <map>
#include <string>

#include "pkcs11/cryptoki.h"

namespace chaps {

typedef std::map<CK_MECHANISM_TYPE, CK_MECHANISM_INFO> MechanismMap;
typedef std::map<CK_MECHANISM_TYPE, CK_MECHANISM_INFO>::const_iterator
    MechanismMapIterator;

class Session;

// SlotManager is the interface for a slot manager.  This component is
// responsible for maintaining a list of slots and slot information as well as
// maintaining a list of open sessions for each slot.
class SlotManager {
 public:
  virtual int GetSlotCount() const = 0;
  virtual int AddSlot() = 0;
  virtual bool IsTokenPresent(int slot_id) const = 0;
  virtual void GetSlotInfo(int slot_id, CK_SLOT_INFO* slot_info) const = 0;
  virtual void GetTokenInfo(int slot_id, CK_TOKEN_INFO* token_info) const = 0;
  virtual const MechanismMap* GetMechanismInfo(int slot_id) const = 0;
  virtual int OpenSession(int slot_id, bool is_read_only) = 0;
  virtual bool CloseSession(int session_id) = 0;
  virtual void CloseAllSessions(int slot_id) = 0;
  virtual bool GetSession(int session_id, Session** session) const = 0;
};

}  // namespace

#endif  // CHAPS_SLOT_MANAGER_H

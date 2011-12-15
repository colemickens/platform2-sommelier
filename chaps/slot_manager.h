// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

// SlotManager is the interface for a slot manager. This component is
// responsible for maintaining a list of slots and slot information as well as
// maintaining a list of open sessions for each slot. See PKCS #11 v2.20: 6.3
// and 11.5 for details on PKCS #11 slots. See sections 6.7 and 11.6 for details
// on PKCS #11 sessions.
class SlotManager {
 public:
  virtual ~SlotManager() {}
  // Returns the total number of slots available. A slot is identified by zero-
  // based offset. I.e. If there are two slots, 0 and 1 are valid 'slot_id'
  // values.
  virtual int GetSlotCount() const = 0;
  virtual bool IsTokenPresent(int slot_id) const = 0;
  virtual void GetSlotInfo(int slot_id, CK_SLOT_INFO* slot_info) const = 0;
  virtual void GetTokenInfo(int slot_id, CK_TOKEN_INFO* token_info) const = 0;
  virtual const MechanismMap* GetMechanismInfo(int slot_id) const = 0;
  // Opens a new session with the token in the given slot. A token must be
  // present. A new and unique session identifier is returned.
  virtual int OpenSession(int slot_id, bool is_read_only) = 0;
  virtual bool CloseSession(int session_id) = 0;
  virtual void CloseAllSessions(int slot_id) = 0;
  virtual bool GetSession(int session_id, Session** session) const = 0;
};

}  // namespace

#endif  // CHAPS_SLOT_MANAGER_H

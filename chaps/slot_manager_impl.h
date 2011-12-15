// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_SLOT_MANAGER_IMPL_H
#define CHAPS_SLOT_MANAGER_IMPL_H

#include "chaps/login_event_listener.h"
#include "chaps/slot_manager.h"

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include <base/basictypes.h>

#include "chaps/chaps_factory.h"
#include "chaps/object_pool.h"

namespace chaps {

class SessionFactory;
class TPMUtility;

class SlotManagerImpl : public SlotManager, public LoginEventListener {
 public:
  SlotManagerImpl(ChapsFactory* session_factory, TPMUtility* tpm_utility);
  virtual ~SlotManagerImpl();
  virtual bool Init();

  // SlotManager methods.
  virtual int GetSlotCount() const;
  virtual bool IsTokenPresent(int slot_id) const;
  virtual void GetSlotInfo(int slot_id, CK_SLOT_INFO* slot_info) const;
  virtual void GetTokenInfo(int slot_id, CK_TOKEN_INFO* token_info) const;
  virtual const MechanismMap* GetMechanismInfo(int slot_id) const;
  virtual int OpenSession(int slot_id, bool is_read_only);
  virtual bool CloseSession(int session_id);
  virtual void CloseAllSessions(int slot_id);
  virtual bool GetSession(int session_id, Session** session) const;

  // LoginEventListener methods.
  virtual void OnLogin(const std::string& path, const std::string& auth_data);
  virtual void OnLogout(const std::string& path);
  virtual void OnChangeAuthData(const std::string& path,
                                const std::string& old_auth_data,
                                const std::string& new_auth_data);
 private:
  enum InternalBlob {
    kEncryptedAuthKey,
    kEncryptedMasterKey
  };
  struct Slot {
    CK_SLOT_INFO slot_info_;
    CK_TOKEN_INFO token_info_;
    std::tr1::shared_ptr<ObjectPool> token_object_pool_;
    std::map<int, std::tr1::shared_ptr<Session> > sessions_;
  };

  ChapsFactory* factory_;
  int last_session_id_;
  MechanismMap mechanism_info_;
  std::map<std::string, int> path_slot_map_;
  std::vector<Slot> slot_list_;
  std::map<int, int> session_slot_map_;
  TPMUtility* tpm_utility_;

  void GetDefaultInfo(CK_SLOT_INFO* slot_info, CK_TOKEN_INFO* token_info);
  bool InitializeKeyHierarchy(ObjectPool* object_pool,
                              const std::string& auth_data,
                              std::string* user_key);
  int FindEmptySlot();
  void AddSlots(int num_slots);

  DISALLOW_COPY_AND_ASSIGN(SlotManagerImpl);
};

}  // namespace

#endif  // CHAPS_SLOT_MANAGER_IMPL_H

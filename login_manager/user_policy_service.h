// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_USER_POLICY_SERVICE_H_
#define LOGIN_MANAGER_USER_POLICY_SERVICE_H_

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>

#include "login_manager/policy_service.h"

namespace login_manager {

class PolicyKey;
class PolicyStore;
class SystemUtils;

// Policy service implementation for user policy.
class UserPolicyService : public PolicyService {
 public:
  UserPolicyService(scoped_ptr<PolicyStore> policy_store,
                    scoped_ptr<PolicyKey> policy_key,
                    const FilePath& key_copy_path,
                    const scoped_refptr<base::MessageLoopProxy>& main_loop,
                    SystemUtils* system_utils);
  virtual ~UserPolicyService();

  // Persists a copy of |scoped_policy_key_| at |key_copy_path_|, if both the
  // key and the copy path are present.
  void PersistKeyCopy();

  // Store a new policy. The only difference from the base PolicyService is that
  // this override allows storage of policy blobs that indiciate the user is
  // unmanaged even if they are unsigned. If an non-signed blob gets installed,
  // we also clear the signing key.
  virtual bool Store(const uint8* policy_blob,
                     uint32 len,
                     Completion* completion,
                     int flags);

  // Invoked after a new key has been persisted. This creates a copy of the key
  // at |key_copy_path_| that is readable by chronos, and notifies the delegate.
  virtual void OnKeyPersisted(bool status);

 private:
  // UserPolicyService owns its PolicyKey, note that PolicyService just keeps a
  // plain pointer.
  scoped_ptr<PolicyKey> scoped_policy_key_;

  // If non-empty then a copy of |scoped_policy_key_| will be stored at this
  // path, readable by chronos.
  FilePath key_copy_path_;

  // Owned by our owner.
  SystemUtils* system_utils_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_USER_POLICY_SERVICE_H_

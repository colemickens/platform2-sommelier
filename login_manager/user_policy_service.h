// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_USER_POLICY_SERVICE_H_
#define LOGIN_MANAGER_USER_POLICY_SERVICE_H_

#include <base/basictypes.h>

#include "login_manager/policy_service.h"

namespace login_manager {

class OwnerKey;
class PolicyStore;

// Policy service implementation for user policy.
class UserPolicyService : public PolicyService {
 public:
  UserPolicyService(PolicyStore* policy_store,
                    OwnerKey* policy_key,
                    const scoped_refptr<base::MessageLoopProxy>& main_loop);
  virtual ~UserPolicyService();

  // Store a new policy. The only difference from the base PolicyService is that
  // this override allows storage of policy blobs that indiciate the user is
  // unmanaged even if they are unsigned. If an non-signed blob gets installed,
  // we also clear the signing key.
  virtual bool Store(const uint8* policy_blob,
                     uint32 len,
                     Completion* completion,
                     int flags);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_USER_POLICY_SERVICE_H_

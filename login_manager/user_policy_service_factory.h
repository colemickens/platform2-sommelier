// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_USER_POLICY_SERVICE_FACTORY_H_
#define LOGIN_MANAGER_USER_POLICY_SERVICE_FACTORY_H_

#include <sys/types.h>

#include <string>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace login_manager {
class PolicyService;

// Factory for creating user policy service instances. User policies are stored
// in the root-owned part of the user's cryptohome.
class UserPolicyServiceFactory {
 public:
  UserPolicyServiceFactory(
      uid_t uid,
      const scoped_refptr<base::MessageLoopProxy>& main_loop);
  virtual ~UserPolicyServiceFactory();

  // Creates a new user policy service instance.
  virtual PolicyService* Create(const std::string& username);

 private:
  // UID to check for.
  uid_t uid_;

  scoped_refptr<base::MessageLoopProxy> main_loop_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyServiceFactory);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_USER_POLICY_SERVICE_FACTORY_H_

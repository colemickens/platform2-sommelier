// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DEVICE_POLICY_H_
#define LOGIN_MANAGER_DEVICE_POLICY_H_

#include <glib.h>
#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>

#include "login_manager/bindings/device_management_backend.pb.h"

namespace login_manager {
class OwnerKey;

// This class holds device settings that are to be enforced across all users.
//
// If there is a policy on disk at creation time, we will load it
// along with its signature.  A new policy and its attendant signature can
// be set at any time and persisted to disk on-demand.
//
// THIS CLASS DOES NO SIGNATURE VALIDATION.
class DevicePolicy {
 public:
  explicit DevicePolicy(const FilePath& policy_path);
  virtual ~DevicePolicy();

  // Load the signed policy off of disk into |policy_|.
  // Returns true unless there is a policy on disk and loading it fails.
  virtual bool LoadOrCreate();

  virtual const enterprise_management::PolicyFetchResponse& Get() const;

  // Persist |policy_| to disk at |policy_file_|
  // Returns false if there's an error while writing data.
  virtual bool Persist();

  virtual bool SerializeToString(std::string* output) const;

  // Clobber the stored policy with new data.
  virtual void Set(const enterprise_management::PolicyFetchResponse& policy);

  // Assuming the current user has access to the owner private key
  // (read: is the owner), this call whitelists |current_user_| and sets a
  // property indicating |current_user_| is the owner in the current policy
  // and schedules a PersistPolicy().
  // Returns false on failure, with |error| set appropriately.
  // |error| can be NULL, should you wish to ignore the particulars.
  bool StoreOwnerProperties(OwnerKey* key,
                            const std::string& current_user,
                            GError** error);

  // Returns true if the current user is listed in |policy_| as the
  // device owner.  Returns false if not, or if that cannot be determined.
  bool CurrentUserIsOwner(const std::string& current_user);

  static const char kDefaultPath[];
  // Format of this string is documented in device_management_backend.proto.
  static const char kDevicePolicyType[];

 private:
  enterprise_management::PolicyFetchResponse policy_;
  const FilePath policy_path_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicy);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_DEVICE_POLICY_H_

// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DEVICE_POLICY_H_
#define LOGIN_MANAGER_DEVICE_POLICY_H_

#include <string>

#include <base/basictypes.h>
#include <base/file_path.h>

namespace login_manager {

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

  virtual const std::string& Get();

  // Persist |policy_| to disk at |policy_file_|
  // Returns false if there's an error while writing data.
  virtual bool Persist();

  // Clobber the stored policy with new data.
  virtual void Set(const std::string& policy);

  static const char kDefaultPath[];

 private:
  std::string policy_;
  const FilePath policy_path_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicy);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_DEVICE_POLICY_H_

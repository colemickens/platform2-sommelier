// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POLICY_LIBPOLICY_H_
#define POLICY_LIBPOLICY_H_

#include <gtest/gtest_prod.h>
#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

class FilePath;

#pragma GCC visibility push(default)

namespace policy {

class DevicePolicy;

// This class holds device settings that are to be enforced across all users.
//
// If there is a policy on disk at creation time, we will load it at verify
// its signature.
class PolicyProvider {
 public:
  PolicyProvider();
  virtual ~PolicyProvider();

  // Contructor for tests only!
  PolicyProvider(DevicePolicy* device_policy);

  // This function will ensure the freshnes of the contents that the getters
  // are delivering. Normally contents are cached to prevent unneccessary load.
  bool Reload();

  bool device_policy_is_loaded() const;

  // Returns a value from the device policy cache.
  virtual const DevicePolicy& GetDevicePolicy() const;

 private:
  scoped_ptr<DevicePolicy> device_policy_;
  bool device_policy_is_loaded_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvider);
};
}  // namespace policy

#pragma GCC visibility pop

#endif  // POLICY_LIBPOLICY_H_

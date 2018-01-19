// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTHPOLICY_WINDOWS_POLICY_MANAGER_H_
#define AUTHPOLICY_WINDOWS_POLICY_MANAGER_H_

#include <memory>

#include <base/files/file_path.h>
#include <base/macros.h>

#include "authpolicy/proto_bindings/active_directory_info.pb.h"

namespace authpolicy {

namespace protos {
class WindowsPolicy;
}  // namespace protos

// Class for managing the lifetime and storage of a protos::WindowsPolicy
// object.
class WindowsPolicyManager {
 public:
  // Constructor. |policy_path| is the file path of the Windows policy when
  // stored on or loaded from disk.
  explicit WindowsPolicyManager(const base::FilePath& policy_path);

  // Loads Windows policy from disk.
  ErrorType LoadFromDisk();

  // Updates the internal policy() object and saves it to disk.
  ErrorType UpdateAndSaveToDisk(std::unique_ptr<protos::WindowsPolicy> policy);

  // Accessor for the WindowsPolicy object. Can be nullptr.
  const protos::WindowsPolicy* policy() const { return policy_.get(); }

  // Wipes |policy_| and the file at |policy_path_|. Returns true if deleting
  // the file worked.
  bool ClearPolicyForTesting();

 private:
  const base::FilePath policy_path_;
  std::unique_ptr<protos::WindowsPolicy> policy_;

  DISALLOW_COPY_AND_ASSIGN(WindowsPolicyManager);
};

}  // namespace authpolicy

#endif  // AUTHPOLICY_WINDOWS_POLICY_MANAGER_H_

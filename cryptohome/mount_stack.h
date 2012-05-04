// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOUNT_STACK_H_
#define CRYPTOHOME_MOUNT_STACK_H_

#include <string>
#include <vector>

// This class is basically a stack that logs an error if it's not empty when
// it's destroyed.
class MountStack {
 public:
  MountStack();
  virtual ~MountStack();

  void Push(const std::string& path);
  bool Pop(std::string* path);
 private:
  std::vector<std::string> mounts_;
};

#endif  // CRYPTOHOME_MOUNT_STACK_H_

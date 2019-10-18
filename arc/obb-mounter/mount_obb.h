// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_OBB_MOUNTER_MOUNT_OBB_H_
#define ARC_OBB_MOUNTER_MOUNT_OBB_H_

#include <string>
// Top-level function to mount obb.
int mount_obb(const char* file_system_name,
              const char* obb_filename,
              const char* mount_path,
              const std::string& owner_uid,
              const std::string& owner_gid);

#endif  // ARC_OBB_MOUNTER_MOUNT_OBB_H_

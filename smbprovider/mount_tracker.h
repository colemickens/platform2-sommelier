// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_MOUNT_TRACKER_H_
#define SMBPROVIDER_MOUNT_TRACKER_H_

#include <base/macros.h>

namespace smbprovider {

class MountTracker {
 public:
  MountTracker();
  ~MountTracker();

  DISALLOW_COPY_AND_ASSIGN(MountTracker);
};

}  // namespace smbprovider

#endif  // SMBPROVIDER_MOUNT_TRACKER_H_

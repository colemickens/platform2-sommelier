// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mount_factory.h"

#include "mount.h"

namespace cryptohome {

MountFactory::MountFactory() { }
MountFactory::~MountFactory() { }

Mount* MountFactory::New() {
  return new Mount();
}
}  // namespace cryptohome

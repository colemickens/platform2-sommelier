// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_MOUNT_FACTORY_H_
#define CRYPTOHOME_MOCK_MOUNT_FACTORY_H_

#include "cryptohome/mount_factory.h"

#include <gmock/gmock.h>

namespace cryptohome {
class Mount;
class MockMountFactory : public MountFactory {
 public:
  MockMountFactory() {
    ON_CALL(*this, New())
        .WillByDefault(testing::Invoke(this, &MockMountFactory::NewConcrete));
  }

  virtual ~MockMountFactory() {}
  MOCK_METHOD0(New, Mount*());

  // Backdoor to access real method, for delegating calls to parent class
  Mount* NewConcrete() { return MountFactory::New(); }
};
}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_MOUNT_FACTORY_H_

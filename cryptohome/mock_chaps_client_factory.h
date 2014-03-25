// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_MOCK_CHAPS_CLIENT_FACTORY_H_
#define CRYPTOHOME_MOCK_CHAPS_CLIENT_FACTORY_H_

#include "chaps_client_factory.h"

namespace cryptohome {

class MockChapsClientFactory : public ChapsClientFactory {
 public:
  MockChapsClientFactory();
  virtual ~MockChapsClientFactory();
  virtual chaps::TokenManagerClient* New();

 protected:
  DISALLOW_COPY_AND_ASSIGN(MockChapsClientFactory);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_MOCK_CHAPS_CLIENT_FACTORY_H_

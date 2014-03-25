// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_CHAPS_CLIENT_FACTORY_H_
#define CRYPTOHOME_CHAPS_CLIENT_FACTORY_H_

#include <base/basictypes.h>

namespace chaps {
class TokenManagerClient;
}

namespace cryptohome {

class ChapsClientFactory {
 public:
  ChapsClientFactory();
  virtual ~ChapsClientFactory();
  virtual chaps::TokenManagerClient* New();

 protected:
  DISALLOW_COPY_AND_ASSIGN(ChapsClientFactory);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_CHAPS_CLIENT_FACTORY_H_

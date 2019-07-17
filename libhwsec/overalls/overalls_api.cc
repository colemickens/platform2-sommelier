// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libhwsec/overalls/overalls_api.h"

#include "libhwsec/overalls/overalls_singleton.h"

namespace hwsec {
namespace overalls {

Overalls* GetOveralls() {
  return OverallsSingleton::GetInstance();
}

}  // namespace overalls
}  // namespace hwsec

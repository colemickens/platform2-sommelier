// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libhwsec/overalls/overalls_singleton.h"

namespace hwsec {
namespace overalls {

Overalls* OverallsSingleton::overalls_ = new Overalls();

Overalls* OverallsSingleton::GetInstance() {
  return overalls_;
}

Overalls* OverallsSingleton::SetInstance(Overalls* ins) {
  Overalls* old_instance = overalls_;
  overalls_ = ins;
  return old_instance;
}

}  // namespace overalls
}  // namespace hwsec

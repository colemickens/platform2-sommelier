// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <shill/resource.h>

namespace shill {
void Resource::Release() {
  freeing_ = 1;
}

bool Resource::IsFreeing() {
  return freeing_;
}
}  // namespace shill

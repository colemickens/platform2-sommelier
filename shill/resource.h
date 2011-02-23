// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RESOURCE_
#define SHILL_RESOURCE_

namespace shill {

class Resource {
 public:
  Resource() : freeing_(false) {}
  void Release();
  bool IsFreeing();

 private:
  bool freeing_;
};

}  // namespace shill

#endif  // SHILL_MANAGER_

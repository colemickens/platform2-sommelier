// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_MANAGER_H_
#define WIMAX_MANAGER_MANAGER_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>

namespace wimax_manager {

class ManagerDBusAdaptor;

class Manager {
 public:
  Manager();
  virtual ~Manager();

 private:
  scoped_ptr<ManagerDBusAdaptor> adaptor_;

  DISALLOW_COPY_AND_ASSIGN(Manager);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_MANAGER_H_

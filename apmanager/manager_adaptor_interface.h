// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MANAGER_ADAPTOR_INTERFACE_H_
#define APMANAGER_MANAGER_ADAPTOR_INTERFACE_H_

#include <base/callback.h>

namespace apmanager {

class ManagerAdaptorInterface {
 public:
  virtual ~ManagerAdaptorInterface() {}

  virtual void RegisterAsync(
      const base::Callback<void(bool)>& completion_callback) = 0;
};

}  // namespace apmanager

#endif  // APMANAGER_MANAGER_ADAPTOR_INTERFACE_H_

// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_ANDROID_CONTAINER_MANAGER_IMPL_H_
#define LOGIN_MANAGER_ANDROID_CONTAINER_MANAGER_IMPL_H_

#include "login_manager/container_manager_impl.h"

#include <string>

#include <base/time/time.h>

namespace login_manager {

class SystemUtils;

// A specialized version of ContainerManagerImpl that provides clean termination
// for the Android container.
class AndroidContainerManagerImpl : public ContainerManagerImpl {
 public:
  AndroidContainerManagerImpl(SystemUtils* system_utils,
                          const base::FilePath& containers_directory,
                          const std::string& name);
  ~AndroidContainerManagerImpl() override = default;

 protected:
  // ContainerManagerImpl:
  bool RequestTermination() override;
  void OnContainerStopped(bool clean) override;

 private:
  // The start time of the shutdown process.
  base::TimeTicks shutdown_start_time_;

  DISALLOW_COPY_AND_ASSIGN(AndroidContainerManagerImpl);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_ANDROID_CONTAINER_MANAGER_IMPL_H_

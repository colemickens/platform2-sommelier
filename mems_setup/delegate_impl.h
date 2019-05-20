// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEMS_SETUP_DELEGATE_IMPL_H_
#define MEMS_SETUP_DELEGATE_IMPL_H_

#include <map>
#include <string>

#include "mems_setup/delegate.h"

namespace mems_setup {

// This is an implementation detail of the DelegateImpl, but it is made
// visible for testing purposes
bool LoadVpdFromString(const std::string& vpd_data,
                       std::map<std::string, std::string>* cache);

class DelegateImpl : public Delegate {
 public:
  DelegateImpl() = default;

  base::Optional<std::string> ReadVpdValue(const std::string& key) override;
  bool ProbeKernelModule(const std::string& module) override;

  bool Exists(const base::FilePath&) override;

 private:
  void LoadVpdIfNeeded();

  std::map<std::string, std::string> vpd_cache_;
  bool vpd_loaded_ = false;
};

}  // namespace mems_setup

#endif  // MEMS_SETUP_DELEGATE_IMPL_H_

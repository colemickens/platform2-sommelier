// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modemfwd/modem_helper_directory_stub.h"

#include <memory>
#include <string>
#include <utility>

namespace modemfwd {

void ModemHelperDirectoryStub::AddHelper(const std::string& device_id,
                                         std::unique_ptr<ModemHelper> helper) {
  helpers_[device_id] = std::move(helper);
}

ModemHelper* ModemHelperDirectoryStub::GetHelperForDeviceId(
    const std::string& device_id) {
  auto it = helpers_.find(device_id);
  if (it == helpers_.end())
    return nullptr;

  return it->second.get();
}

}  // namespace modemfwd

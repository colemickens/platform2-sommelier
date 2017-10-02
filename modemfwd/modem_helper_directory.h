// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_MODEM_HELPER_DIRECTORY_H_
#define MODEMFWD_MODEM_HELPER_DIRECTORY_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>

namespace modemfwd {

class ModemHelper;

class ModemHelperDirectory {
 public:
  virtual ~ModemHelperDirectory() = default;

  // Returns a weak pointer, do not store or free.
  virtual ModemHelper* GetHelperForDeviceId(const std::string& device_id) = 0;
};

std::unique_ptr<ModemHelperDirectory> CreateModemHelperDirectory(
    const base::FilePath& directory);

}  // namespace modemfwd

#endif  // MODEMFWD_MODEM_HELPER_DIRECTORY_H_

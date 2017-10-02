// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODEMFWD_MODEM_HELPER_DIRECTORY_STUB_H_
#define MODEMFWD_MODEM_HELPER_DIRECTORY_STUB_H_

#include <map>
#include <memory>
#include <string>

#include <base/macros.h>

#include "modemfwd/modem_helper.h"
#include "modemfwd/modem_helper_directory.h"

namespace modemfwd {

class ModemHelperDirectoryStub : public ModemHelperDirectory {
 public:
  ModemHelperDirectoryStub() = default;
  ~ModemHelperDirectoryStub() override = default;

  void AddHelper(const std::string& device_id,
                 std::unique_ptr<ModemHelper> helper);

  // modemfwd::ModemHelperDirectory overrides.
  ModemHelper* GetHelperForDeviceId(const std::string& device_id) override;

 private:
  std::map<std::string, std::unique_ptr<ModemHelper>> helpers_;

  DISALLOW_COPY_AND_ASSIGN(ModemHelperDirectoryStub);
};

}  // namespace modemfwd

#endif  // MODEMFWD_MODEM_HELPER_DIRECTORY_STUB_H_

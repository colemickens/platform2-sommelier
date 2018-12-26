/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef RUNTIME_PROBE_FUNCTIONS_GENERIC_STORAGE_H_
#define RUNTIME_PROBE_FUNCTIONS_GENERIC_STORAGE_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/values.h>

#include "runtime_probe/probe_function.h"

namespace runtime_probe {

class GenericStorageFunction : public ProbeFunction {
 public:
  static constexpr auto function_name = "generic_storage";

  static std::unique_ptr<ProbeFunction> FromDictionaryValue(
      const base::DictionaryValue& dict_value) {
    std::unique_ptr<GenericStorageFunction> instance{
        new GenericStorageFunction()};

    if (dict_value.size() != 0) {
      LOG(ERROR) << function_name << " dooes not take any arguement";
      return nullptr;
    }
    return instance;
  }

  // Override `Eval` function, which should return a list of DictionaryValue
  DataType Eval() const override;

 private:
  static ProbeFunction::Register<GenericStorageFunction> register_;

  // Get the result of mmc extcsd on the device sepcified by |node path| and put
  // it into |output|
  bool GetOutputOfMmcExtcsd(const base::FilePath& node_path,
                            std::string* output) const;

  // Return all the fixed storage devices.
  std::vector<base::FilePath> GetFixedDevices() const;

  // Extracts eMMC 5.0 firmware version of storage device specified by
  // |node_path| from EXT_CSD[254:262]
  // TODO(hmchu): Currently mmc D-Bus call to debugd only retrives info of
  // /dev/mmcblk0. Retrieve mmc info based on |node_path| once the corresponding
  // D-Bus call is ready.
  std::string GetEMMC5FirmwareVersion(const base::FilePath& node_path) const;
};

/* Register the GenericStorageFunction */
REGISTER_PROBE_FUNCTION(GenericStorageFunction);

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_FUNCTIONS_GENERIC_STORAGE_H_

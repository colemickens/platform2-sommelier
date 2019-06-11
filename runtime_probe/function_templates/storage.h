// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RUNTIME_PROBE_FUNCTION_TEMPLATES_STORAGE_H_
#define RUNTIME_PROBE_FUNCTION_TEMPLATES_STORAGE_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/optional.h>
#include <base/values.h>

#include "runtime_probe/probe_function.h"

namespace runtime_probe {

class StorageFunction : public ProbeFunction {
 public:
  // This class is a template for storage probing workflow and should never be
  // instantiated.
  static std::unique_ptr<ProbeFunction> FromDictionaryValue(
      const base::DictionaryValue& dict_value) = delete;

  // Override `Eval` function, which should return a list of DictionaryValue
  DataType Eval() const final;

 protected:
  StorageFunction() = default;
  // The following are storage-type specific building blocks.
  // Must be implemented on each derived storage probe function class.

  // Eval the storage indicated by |node_path|. Return empty DictionaryValue if
  // storage type indicated by |node_path| does not match the target type. On
  // ther other hand, if the storage type matches the target type, the return
  // DictionaryValue must contain at least the "type" key.
  virtual base::DictionaryValue EvalByPath(
      const base::FilePath& node_path) const = 0;

 private:
  // The following functions are shared across different types of storage.
  std::vector<base::FilePath> GetFixedDevices() const;
  base::Optional<int64_t> GetStorageSectorCount(
      const base::FilePath& node_path) const;
  int32_t GetLogicalBlockSize() const;
};

}  // namespace runtime_probe

#endif  // RUNTIME_PROBE_FUNCTION_TEMPLATES_STORAGE_H_

// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "runtime_probe/function_templates/storage.h"

#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/optional.h>
#include <brillo/strings/string_utils.h>

#include "runtime_probe/utils/file_utils.h"

namespace runtime_probe {
namespace {
constexpr auto kStorageDirPath("/sys/class/block/");
constexpr auto kReadFileMaxSize = 1024;
// TODO(b/123097249): Remove the following hard-coded constant once
// authenticated source of storage size to use is determined
constexpr auto kBytesPerSector = 512;
}  // namespace

// Get paths of all non-removeable physical storage
std::vector<base::FilePath> StorageFunction::GetFixedDevices() const {
  std::vector<base::FilePath> res{};
  const base::FilePath storage_dir_path(kStorageDirPath);
  base::FileEnumerator storage_dir_it(storage_dir_path, true,
                                      base::FileEnumerator::SHOW_SYM_LINKS |
                                          base::FileEnumerator::FILES |
                                          base::FileEnumerator::DIRECTORIES);

  while (true) {
    const auto storage_path = storage_dir_it.Next();
    if (storage_path.empty())
      break;
    // Only return non-removable devices
    const auto storage_removable_path = storage_path.Append("removable");
    std::string removable_res;
    if (!base::ReadFileToString(storage_removable_path, &removable_res)) {
      VLOG(2) << "Storage device " << storage_path.value()
              << " does not specify the removable property. May be a partition "
                 "of a storage device.";
      continue;
    }

    if (base::TrimWhitespaceASCII(removable_res, base::TrimPositions::TRIM_ALL)
            .as_string() != "0") {
      VLOG(2) << "Storage device " << storage_path.value() << " is removable.";
      continue;
    }

    // Skip loobpack or dm-verity device
    if (base::StartsWith(storage_path.BaseName().value(), "loop",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(storage_path.BaseName().value(), "dm-",
                         base::CompareCase::SENSITIVE))
      continue;

    res.push_back(storage_path);
  }

  return res;
}

// Get storage size based on |node_path|
base::Optional<int64_t> StorageFunction::GetStorageSectorCount(
    const base::FilePath& node_path) const {
  // The sysfs entry for size info
  const auto size_path = node_path.Append("size");
  std::string size_content;
  if (!base::ReadFileToStringWithMaxSize(size_path, &size_content,
                                         kReadFileMaxSize)) {
    LOG(WARNING) << "Storage device " << node_path.value()
                 << " does not specify size.";
    return base::nullopt;
  }

  const auto sector_str =
      base::TrimWhitespaceASCII(size_content, base::TrimPositions::TRIM_ALL);
  int64_t sector_int;
  if (!base::StringToInt64(sector_str, &sector_int)) {
    LOG(ERROR) << "Failed to parse recorded sector of" << node_path.value()
               << " to integer!";
    return base::nullopt;
  }

  return sector_int;
}

// Get logical block size of storage. Currently hard-coded.
int32_t StorageFunction::GetLogicalBlockSize() const {
  return kBytesPerSector;
}

StorageFunction::DataType StorageFunction::Eval() const {
  const auto storage_nodes_path_list = GetFixedDevices();
  DataType result{};

  for (const auto& node_path : storage_nodes_path_list) {
    VLOG(2) << "Processnig the node " << node_path.value();

    // Get type specific fields and their values
    auto node_res = EvalByPath(node_path);
    if (node_res.empty())
      continue;

    // Report the absolute path we probe the reported info from
    node_res.SetString("path", node_path.value());

    // Get size of storage
    const auto sector_count = GetStorageSectorCount(node_path);
    const int32_t logical_block_size = GetLogicalBlockSize();
    if (!sector_count) {
      node_res.SetString("sectors", "-1");
      node_res.SetString("size", "-1");
    } else {
      node_res.SetString("sectors", base::Int64ToString(sector_count.value()));
      node_res.SetString("size", base::Int64ToString(sector_count.value() *
                                                     logical_block_size));
    }

    result.push_back(std::move(node_res));
  }
  return result;
}

}  // namespace runtime_probe

/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/strings/string_split.h>
#include <brillo/strings/string_utils.h>

#include "runtime_probe/functions/generic_storage.h"
#include "runtime_probe/utils/file_utils.h"

namespace runtime_probe {
namespace {
constexpr auto kStorageDirPath("/sys/class/block/");
constexpr auto kReadFileMaxSize = 1024;

const std::vector<std::string> kAtaFields{"vendor", "model"};
const std::vector<std::string> kEmmcFields{"type", "name", "oemid", "manfid",
                                           "serial"};
// Attributes in optional fields:
// prv: SD and MMCv4 only
// hwrev: SD and MMCv1 only
const std::vector<std::string> kEmmcOptionalFields{"prv", "hwrev"};
const std::vector<std::string> kNvmeFields{"vendor", "device", "class"};

}  // namespace

std::vector<base::FilePath> GenericStorageFunction::GetFixedDevices() const {
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
      VLOG(1) << "Storage device " << storage_path.value()
              << " does not specify the removable property. Assume removable.";
      continue;
    }

    if (base::TrimWhitespaceASCII(removable_res, base::TrimPositions::TRIM_ALL)
            .as_string() != "0") {
      VLOG(1) << "Storage device " << storage_path.value() << " is removable.";
      continue;
    }

    // Skip Loobpack or dm-verity device
    if (base::StartsWith(storage_path.BaseName().value(), "loop",
                         base::CompareCase::SENSITIVE) ||
        base::StartsWith(storage_path.BaseName().value(), "dm-",
                         base::CompareCase::SENSITIVE))
      continue;

    res.push_back(storage_path);
  }

  return res;
}

// TODO(hmchu): Retrieve real eMMC firmware version
std::string GenericStorageFunction::GetEMMC5FirmwareVersion(
    const base::FilePath& node_path) const {
  return std::string{""};
}

GenericStorageFunction::DataType GenericStorageFunction::Eval() const {
  const auto storage_nodes_path_list = GetFixedDevices();
  DataType result{};

  for (const auto& node_path : storage_nodes_path_list) {
    VLOG(1) << "Processnig the node: " << node_path.value();
    const auto dev_path = node_path.Append("device");
    // For NVMe device, "/<node_path>/device/device/.." is expected.
    const auto nvme_dev_path = dev_path.Append("device");

    base::DictionaryValue node_res{};

    // dev_path is the paraent directory of nvme_dev_path
    if (!base::PathExists(dev_path)) {
      VLOG(1) << "None of ATA, NVMe or eMMC fields exist on storage device "
              << node_path.value();
      continue;
    }

    // ATA, NVMe and eMMC are mutually exclusive indicators
    // TODO(b/122027599): Add "ATA" to field "type" if ata_res is not empty
    const base::DictionaryValue ata_res =
        MapFilesToDict(dev_path, kAtaFields, {});
    const base::DictionaryValue emmc_res =
        MapFilesToDict(dev_path, kEmmcFields, kEmmcOptionalFields);

    if (!emmc_res.empty()) {
      // Get eMMC 5.0 firmaware version
      const auto emmc5_fw_ver = GetEMMC5FirmwareVersion(node_path);
      if (!emmc5_fw_ver.empty())
        node_res.SetString("emmc5_fw_ver", emmc5_fw_ver);
    }

    node_res.MergeDictionary(&ata_res);
    node_res.MergeDictionary(&emmc_res);

    if (base::PathExists(nvme_dev_path)) {
      // TODO(b/122027599): Add "NVMe" to field "type" if nvme_res is not empty
      const base::DictionaryValue nvme_res =
          MapFilesToDict(nvme_dev_path, kNvmeFields, {});
      node_res.MergeDictionary(&nvme_res);
    }

    if (node_res.empty()) {
      VLOG(1) << "Cannot probe ATA, NVMe or eMMC fields under  "
              << dev_path.value() << " or " << nvme_dev_path.value();
      continue;
    }

    // Size info
    const auto size_path = node_path.Append("size");
    std::string size_content;
    if (base::ReadFileToStringWithMaxSize(size_path, &size_content,
                                          kReadFileMaxSize)) {
      node_res.SetString(
          "sectors", base::TrimWhitespaceASCII(size_content,
                                               base::TrimPositions::TRIM_ALL));
    } else {
      VLOG(1) << "Storage device " << node_path.value()
              << " does not specify size";
      node_res.SetString("sectors", "-1");
    }

    result.emplace_back();
    result.back().Swap(&node_res);
  }

  return result;
}

}  // namespace runtime_probe

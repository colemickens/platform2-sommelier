// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "runtime_probe/functions/generic_storage.h"

namespace runtime_probe {

std::unique_ptr<ProbeFunction> GenericStorageFunction::FromDictionaryValue(
    const base::DictionaryValue& dict_value) {
  std::unique_ptr<GenericStorageFunction> instance{
      new GenericStorageFunction()};
  if (dict_value.size() != 0) {
    LOG(ERROR) << function_name << " does not take any argument";
    return nullptr;
  }
  instance->ata_prober_ = std::make_unique<AtaStorageFunction>();
  instance->mmc_prober_ = std::make_unique<MmcStorageFunction>();
  instance->nvme_prober_ = std::make_unique<NvmeStorageFunction>();

  return instance;
}

base::DictionaryValue GenericStorageFunction::EvalByDV(
    const base::DictionaryValue& storage_dv) const {
  std::string storage_type;

  if (!storage_dv.GetString("type", &storage_type)) {
    LOG(ERROR) << "No \"type\" field in current storage DictionaryValue.";
    return {};
  }
  if (storage_type == "ATA")
    return ata_prober_->EvalByDV(storage_dv);
  if (storage_type == "MMC")
    return mmc_prober_->EvalByDV(storage_dv);
  if (storage_type == "NVMe")
    return nvme_prober_->EvalByDV(storage_dv);
  LOG(WARNING) << "Type \"" << storage_type << "\" not recognized";
  return {};
}

base::DictionaryValue GenericStorageFunction::EvalInHelperByPath(
    const base::FilePath& node_path) const {
  VLOG(1) << "Trying to determine the type of storage device \""
          << node_path.value() << "\"";

  base::DictionaryValue storage_res{};
  storage_res = ata_prober_->EvalInHelperByPath(node_path);
  if (storage_res.empty())
    storage_res = mmc_prober_->EvalInHelperByPath(node_path);
  if (storage_res.empty())
    storage_res = nvme_prober_->EvalInHelperByPath(node_path);
  return storage_res;
}
}  // namespace runtime_probe

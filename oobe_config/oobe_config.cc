// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/oobe_config.h"

#include <base/files/file_util.h>
#include <base/logging.h>
#include <map>
#include <policy/resilient_policy_util.h>

#include "oobe_config/oobe_config_constants.h"
#include "oobe_config/rollback_data.pb.h"

namespace oobe_config {

base::FilePath OobeConfig::GetPrefixedFilePath(
    const base::FilePath& file_path) {
  if (prefix_path_for_testing_.empty())
    return file_path;
  DCHECK(!file_path.value().empty());
  DCHECK_EQ('/', file_path.value()[0]);
  return prefix_path_for_testing_.Append(file_path.value().substr(1));
}

bool OobeConfig::ReadFileWithoutPrefix(const base::FilePath& file_path,
                                       std::string* out_content) {
  bool result = base::ReadFileToString(file_path, out_content);
  if (result) {
    LOG(INFO) << "Loaded " << file_path.value();
  } else {
    LOG(ERROR) << "Couldn't read " << file_path.value();
    *out_content = "";
  }
  return result;
}

bool OobeConfig::ReadFile(const base::FilePath& file_path,
                          std::string* out_content) {
  return ReadFileWithoutPrefix(GetPrefixedFilePath(file_path), out_content);
}

bool OobeConfig::WriteFileWithoutPrefix(const base::FilePath& file_path,
                                        const std::string& data) {
  if (!base::CreateDirectory(file_path.DirName())) {
    LOG(ERROR) << "Couldn't create directory for " << file_path.value();
    return false;
  }
  int bytes_written = base::WriteFile(file_path, data.c_str(), data.size());
  if (bytes_written != data.size()) {
    LOG(ERROR) << "Couldn't write " << file_path.value();
    return false;
  }
  LOG(INFO) << "Wrote " << file_path.value();
  return true;
}

bool OobeConfig::WriteFile(const base::FilePath& file_path,
                           const std::string& data) {
  return WriteFileWithoutPrefix(GetPrefixedFilePath(file_path), data);
}

bool OobeConfig::GetRollbackData(RollbackData* rollback_data) {
  std::string file_content;
  ReadFile(kInstallAttributesPath, &file_content);
  rollback_data->set_install_attributes(file_content);
  ReadFile(kOwnerKeyPath, &file_content);
  rollback_data->set_owner_key(file_content);
  ReadFile(kShillDefaultProfilePath, &file_content);
  rollback_data->set_shill_default_profile(file_content);

  PolicyData* policy_data = rollback_data->mutable_device_policy();
  std::map<int, base::FilePath> policy_paths =
      policy::GetSortedResilientPolicyFilePaths(
          GetPrefixedFilePath(kPolicyPath));
  for (auto entry : policy_paths) {
    policy_data->add_policy_index(entry.first);
    ReadFileWithoutPrefix(entry.second, &file_content);
    policy_data->add_policy_file(file_content);
  }
  return true;
}

bool OobeConfig::RestoreRollbackData(const RollbackData& rollback_data) {
  WriteFile(kInstallAttributesPath, rollback_data.install_attributes());
  WriteFile(kOwnerKeyPath, rollback_data.owner_key());
  WriteFile(kShillDefaultProfilePath, rollback_data.shill_default_profile());

  if (rollback_data.device_policy().policy_file_size() !=
      rollback_data.device_policy().policy_index_size()) {
    LOG(ERROR) << "Invalid rollback_data.";
    return false;
  }
  for (int i = 0; i < rollback_data.device_policy().policy_file_size(); ++i) {
    base::FilePath policy_path = policy::GetResilientPolicyFilePathForIndex(
        GetPrefixedFilePath(kPolicyPath),
        rollback_data.device_policy().policy_index(i));
    WriteFileWithoutPrefix(policy_path,
                           rollback_data.device_policy().policy_file(i));
  }

  return true;
}

bool OobeConfig::UnencryptedRollbackSave() {
  RollbackData rollback_data;
  if (!GetRollbackData(&rollback_data)) {
    return false;
  }

  // Write proto to an unencrypted file.
  std::string rollback_data_str;
  if (!rollback_data.SerializeToString(&rollback_data_str)) {
    LOG(ERROR) << "Couldn't serialize proto.";
    return false;
  }
  if (!WriteFile(kRollbackDataPath, rollback_data_str)) {
    return false;
  }

  return true;
}

bool OobeConfig::UnencryptedRollbackRestore() {
  std::string rollback_data_str;
  if (!ReadFile(kRollbackDataPath, &rollback_data_str)) {
    return false;
  }
  RollbackData rollback_data;
  if (!rollback_data.ParseFromString(rollback_data_str)) {
    LOG(ERROR) << "Couldn't parse proto.";
    return false;
  }
  LOG(INFO) << "Parsed " << kRollbackDataPath.value();

  // Data is already unencrypted, restore it.
  return RestoreRollbackData(rollback_data);
}

}  // namespace oobe_config

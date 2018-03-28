// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <brillo/blkdev_utils/device_mapper.h>

#include <libdevmapper.h>
#include <algorithm>
#include <utility>

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#include <brillo/blkdev_utils/device_mapper_task.h>
#include <brillo/secure_blob.h>

namespace brillo {

DevmapperTable::DevmapperTable(uint64_t start,
                               uint64_t size,
                               const std::string& type,
                               const SecureBlob& parameters)
    : start_(start), size_(size), type_(type), parameters_(parameters) {}

SecureBlob DevmapperTable::ToBlob() {
  SecureBlob table_blob(base::StringPrintf("%" PRIu64 " %" PRIu64 " %s ",
                                           start_, size_, type_.c_str()));

  return SecureBlob::Combine(table_blob, parameters_);
}

DevmapperTable DevmapperTable::CreateTableFromBlob(const SecureBlob& table) {
  int n;
  uint64_t start, size;
  char ttype[128];
  const char* table_char_data = table.char_data();

  // Devmapper table is of the form {start, size, type, parameters}.
  if (sscanf(table_char_data, "%" PRIu64 " %" PRIu64 " %127s %n", &start, &size,
             ttype, &n) != 3)
    return DevmapperTable(0, 0, "", SecureBlob());

  // After scanning 3 words, the rest of the string represents parameters.
  uint64_t remaining_bytes = table.size() - n;

  if (remaining_bytes == 0)
    return DevmapperTable(0, 0, "", SecureBlob());

  SecureBlob target(remaining_bytes);
  const char* parameter_str = table_char_data + n;

  for (int i = 0; i < remaining_bytes; i++)
    target[i] = parameter_str[i];

  return DevmapperTable(start, size, std::string(ttype), std::move(target));
}

SecureBlob DevmapperTable::CryptGetKey() {
  int n;
  char cipher[128];

  // Check for non-dmcrypt table.
  if (type_ != "crypt")
    return SecureBlob();

  // First field is the cipher.
  const char* parameters_char_data = parameters_.char_data();
  if (sscanf(parameters_char_data, "%127s %n", cipher, &n) != 1) {
    LOG(INFO) << "Invalid parameters to dmcrypt device.";
    return SecureBlob();
  }

  // Second field is the key.
  uint64_t remaining_bytes = parameters_.size() - n;

  if (remaining_bytes == 0)
    return SecureBlob();

  const char* key_char_data = parameters_char_data + n;
  SecureBlob hex_key(remaining_bytes, 0);

  // Copy relevant bytes and resize the blob.
  int i;
  for (i = 0; i < remaining_bytes; i++) {
    if (key_char_data[i] == ' ')
      break;
    hex_key[i] = key_char_data[i];
  }

  hex_key.resize(i);

  SecureBlob key = SecureHexToBlob(hex_key);

  if (key.empty()) {
    LOG(ERROR) << "CryptExtractKey: HexStringToBytes failed";
    return SecureBlob();
  }

  return key;
}

// In order to not leak the encryption key to non-SecureBlob managed memory,
// create the parameter blobs in three parts and combine.
SecureBlob DevmapperTable::CryptCreateParameters(
    const std::string& cipher,
    const SecureBlob& encryption_key,
    const int iv_offset,
    const base::FilePath& device,
    int device_offset,
    bool allow_discard) {
  SecureBlob parameter_parts[3];

  parameter_parts[0] = SecureBlob(cipher + " ");
  parameter_parts[1] = BlobToSecureHex(encryption_key);
  parameter_parts[2] = SecureBlob(base::StringPrintf(
      " %d %s %d%s", iv_offset, device.value().c_str(), device_offset,
      (allow_discard ? " 1 allow_discards" : "")));

  SecureBlob parameters;
  for (auto param_part : parameter_parts)
    parameters = SecureBlob::Combine(parameters, param_part);

  return parameters;
}

std::unique_ptr<DevmapperTask> CreateDevmapperTask(int type) {
  return std::make_unique<DevmapperTaskImpl>(type);
}

DeviceMapper::DeviceMapper() {
  dm_task_factory_ = base::Bind(&CreateDevmapperTask);
}

DeviceMapper::DeviceMapper(const DevmapperTaskFactory& factory)
    : dm_task_factory_(factory) {}

bool DeviceMapper::Setup(const std::string& name, const DevmapperTable& table) {
  auto task = dm_task_factory_.Run(DM_DEVICE_CREATE);

  if (!task->SetName(name)) {
    LOG(ERROR) << "Setup: SetName failed.";
    return false;
  }

  if (!task->AddTarget(table.GetStart(), table.GetSize(), table.GetType(),
                       table.GetParameters())) {
    LOG(ERROR) << "Setup: AddTarget failed";
    return false;
  }

  if (!task->Run(true /* udev sync */)) {
    LOG(ERROR) << "Setup: Run failed.";
    return false;
  }

  return true;
}

bool DeviceMapper::Remove(const std::string& name) {
  auto task = dm_task_factory_.Run(DM_DEVICE_REMOVE);

  if (!task->SetName(name)) {
    LOG(ERROR) << "Remove: SetName failed.";
    return false;
  }

  if (!task->Run(true /* udev_sync */)) {
    LOG(ERROR) << "Remove: Teardown failed.";
    return false;
  }

  return true;
}

DevmapperTable DeviceMapper::GetTable(const std::string& name) {
  auto task = dm_task_factory_.Run(DM_DEVICE_TABLE);
  uint64_t start, size;
  std::string type;
  SecureBlob parameters;

  if (!task->SetName(name)) {
    LOG(ERROR) << "GetTable: SetName failed.";
    return DevmapperTable(0, 0, "", SecureBlob());
  }

  if (!task->Run()) {
    LOG(ERROR) << "GetTable: Run failed.";
    return DevmapperTable(0, 0, "", SecureBlob());
  }

  task->GetNextTarget(&start, &size, &type, &parameters);

  return DevmapperTable(start, size, type, parameters);
}

bool DeviceMapper::WipeTable(const std::string& name) {
  auto size_task = dm_task_factory_.Run(DM_DEVICE_TABLE);

  if (!size_task->SetName(name)) {
    LOG(ERROR) << "WipeTable: SetName failed.";
    return false;
  }

  if (!size_task->Run()) {
    LOG(ERROR) << "WipeTable: RunTask failed.";
    return false;
  }

  // Arguments for fetching dm target.
  bool ret = false;
  uint64_t start = 0, size = 0, total_size = 0;
  std::string type;
  SecureBlob parameters;

  // Get maximum size of the device to be wiped.
  do {
    ret = size_task->GetNextTarget(&start, &size, &type, &parameters);
    total_size = std::max(start + size, total_size);
  } while (ret);

  // Setup wipe task.
  auto wipe_task = dm_task_factory_.Run(DM_DEVICE_RELOAD);

  if (!wipe_task->SetName(name)) {
    LOG(ERROR) << "WipeTable: SetName failed.";
    return false;
  }

  if (!wipe_task->AddTarget(0, total_size, "error", SecureBlob())) {
    LOG(ERROR) << "WipeTable: AddTarget failed.";
    return false;
  }

  if (!wipe_task->Run()) {
    LOG(ERROR) << "WipeTable: RunTask failed.";
    return false;
  }

  return true;
}

}  // namespace brillo

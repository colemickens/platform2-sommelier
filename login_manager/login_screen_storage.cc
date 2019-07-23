// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/login_screen_storage.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <crypto/sha2.h>
#include <dbus/bus.h>

#include "login_manager/dbus_util.h"
#include "login_manager/secret_util.h"

namespace login_manager {

namespace {

// Create a pipe that contains a given data preceded by its size. In case of
// failure fills |error| accordingly.
bool CreatePipeWithData(brillo::ErrorPtr* error,
                        const std::vector<uint8_t>& data,
                        brillo::dbus_utils::FileDescriptor* out_pipe_fd) {
  *out_pipe_fd = secret_util::WriteSizeAndDataToPipe(data);
  if (!out_pipe_fd->get()) {
    *error = CreateError(DBUS_ERROR_IO_ERROR, "couldn't create a pipe.");
    return false;
  }
  return true;
}

}  // namespace

LoginScreenStorage::LoginScreenStorage(
    const base::FilePath& persistent_storage_path)
    : persistent_storage_path_(persistent_storage_path) {}

bool LoginScreenStorage::Store(brillo::ErrorPtr* error,
                               const std::string& key,
                               const LoginScreenStorageMetadata& metadata,
                               const base::ScopedFD& value_fd) {
  std::vector<uint8_t> value;
  if (!secret_util::ReadSecretFromPipe(value_fd.get(), &value)) {
    *error = CreateError(DBUS_ERROR_IO_ERROR, "couldn't read value from pipe.");
    return false;
  }

  // Removing the old value from both storages to make sure it's not duplicated.
  RemoveKeyFromLoginScreenStorage(key);

  if (metadata.clear_on_session_exit()) {
    in_memory_storage_[key] = std::move(value);
    return true;
  }

  base::FilePath storage_dir_path(persistent_storage_path_);
  if (!base::DirectoryExists(storage_dir_path) &&
      !base::CreateDirectory(storage_dir_path)) {
    *error = CreateError(DBUS_ERROR_IO_ERROR,
                         "couldn't create login screen storage directory.");
    return false;
  }

  if (base::WriteFile(GetPersistentStoragePathForKey(key),
                      reinterpret_cast<char*>(value.data()),
                      value.size()) != value.size()) {
    *error = CreateError(DBUS_ERROR_IO_ERROR,
                         "couldn't write key/value pair to the disk.");
    return false;
  }

  return true;
}

bool LoginScreenStorage::Retrieve(
    brillo::ErrorPtr* error,
    const std::string& key,
    brillo::dbus_utils::FileDescriptor* out_value_fd) {
  auto value_iter = in_memory_storage_.find(key);
  if (value_iter != in_memory_storage_.end()) {
    return CreatePipeWithData(error, value_iter->second, out_value_fd);
  }

  base::FilePath value_path = GetPersistentStoragePathForKey(key);
  std::string value;
  if (!base::PathExists(value_path) ||
      !base::ReadFileToString(value_path, &value)) {
    *error = CreateError(DBUS_ERROR_INVALID_ARGS,
                         "no value was found for the given key.");
    return false;
  }
  return CreatePipeWithData(
      error, std::vector<uint8_t>(value.begin(), value.end()), out_value_fd);
}

base::FilePath LoginScreenStorage::GetPersistentStoragePathForKey(
    const std::string& key) {
  return base::FilePath(persistent_storage_path_)
      .Append(secret_util::StringToSafeFilename(key));
}

void LoginScreenStorage::RemoveKeyFromLoginScreenStorage(
    const std::string& key) {
  in_memory_storage_.erase(key);
  base::DeleteFile(GetPersistentStoragePathForKey(key), /*recursive=*/false);
}

}  //  namespace login_manager

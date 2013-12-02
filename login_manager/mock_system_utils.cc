// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/mock_system_utils.h"

#include <string>

#include <base/file_util.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>

#include "login_manager/scoped_dbus_pending_call.h"

namespace login_manager {

MockSystemUtils::MockSystemUtils() {
}

MockSystemUtils::~MockSystemUtils() {
  EXPECT_TRUE(fake_calls_.empty()) << "CallAsyncMethodOnChromium expected"
                                   << " to be called " << fake_calls_.size()
                                   << " more times.";
}

bool MockSystemUtils::Exists(const FilePath& file) {
  return EnsureTempDir() && real_utils_.Exists(PutInsideTempdir(file));
}

bool MockSystemUtils::AtomicFileWrite(const FilePath& file,
                                      const char* data,
                                      int size) {
  if (!EnsureTempDir())
    return false;
  base::FilePath to_write(PutInsideTempdir(file));
  if (!file_util::CreateDirectory(to_write.DirName())) {
    PLOG(ERROR) << "Could not recursively create "
                << to_write.DirName().value();
    return false;
  }
  return real_utils_.AtomicFileWrite(to_write, data, size);
}

bool MockSystemUtils::ReadFileToString(const FilePath& file,
                                       std::string* out) {
  return EnsureTempDir() && file_util::ReadFileToString(PutInsideTempdir(file),
                                                        out);
}

bool MockSystemUtils::EnsureAndReturnSafeFileSize(const FilePath& file,
                                                  int32* file_size_32) {
  return (EnsureTempDir() &&
          real_utils_.EnsureAndReturnSafeFileSize(PutInsideTempdir(file),
                                                  file_size_32));
}

bool MockSystemUtils::RemoveFile(const FilePath& file) {
  return EnsureTempDir() && real_utils_.RemoveFile(PutInsideTempdir(file));
}

bool MockSystemUtils::GetUniqueFilenameInWriteOnlyTempDir(
    FilePath* temp_file_path) {
  return CreateReadOnlyFileInTempDir(temp_file_path);
}

bool MockSystemUtils::CreateReadOnlyFileInTempDir(FilePath* temp_file_path) {
  *temp_file_path = GetUniqueFilename();
  return !temp_file_path->empty();
}

base::FilePath MockSystemUtils::GetUniqueFilename() {
  if (unique_file_path_.empty()) {
    if (EnsureTempDir() &&
        !file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                             &unique_file_path_)) {
      PLOG(ERROR) << "Could not create file in " << temp_dir_.path().value();
      unique_file_path_.clear();
    }
  }
  return unique_file_path_;
}

scoped_ptr<ScopedDBusPendingCall> MockSystemUtils::CallAsyncMethodOnChromium(
    const char* method_name) {
  if (fake_calls_.empty()) {
    ADD_FAILURE() << "CallAsyncMethodOnChromium called too many times!";
    return scoped_ptr<ScopedDBusPendingCall>(NULL);
  }
  ScopedDBusPendingCall* to_return = fake_calls_[0];
  fake_calls_.weak_erase(fake_calls_.begin());
  return scoped_ptr<ScopedDBusPendingCall>(to_return);
}

void MockSystemUtils::EnqueueFakePendingCall(
    scoped_ptr<ScopedDBusPendingCall> fake_call) {
  fake_calls_.push_back(fake_call.release());
}

bool MockSystemUtils::EnsureTempDir() {
  if (!temp_dir_.IsValid() && !temp_dir_.CreateUniqueTempDir()) {
    PLOG(ERROR) << "Could not create temp dir";
    return false;
  }
  return true;
}

base::FilePath MockSystemUtils::PutInsideTempdir(const base::FilePath& path) {
  base::FilePath to_append(path);
  while (to_append.IsAbsolute()) {
    std::string ascii(path.MaybeAsASCII());
    to_append = base::FilePath(ascii.substr(1, std::string::npos));
  }
  base::FilePath to_return(temp_dir_.path().Append(to_append));
  return to_return;
}

}  // namespace login_manager

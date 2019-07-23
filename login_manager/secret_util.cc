// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/secret_util.h"

#include <utility>

#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <crypto/sha2.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace login_manager {
namespace secret_util {

namespace {

// 64k of data, minus 64 bits for a preceding size. This number was chosen to
// fit all the data in a single pipe buffer and avoid blocking on write.
// (http://man7.org/linux/man-pages/man7/pipe.7.html)
const size_t kDataSizeLimit = 1024 * 64 - sizeof(size_t);

// Reads |data_size| from the file descriptor. Returns 'false' if it couldn't
// read |data_size| or if |data_size| was incorrect.
bool GetSecretDataSize(int in_secret_fd, size_t* data_size_out) {
  if (!base::ReadFromFD(in_secret_fd, reinterpret_cast<char*>(data_size_out),
                        sizeof(size_t))) {
    PLOG(ERROR) << "Could not read secret size from file.";
    return false;
  }

  if (*data_size_out == 0 || *data_size_out > kDataSizeLimit) {
    LOG(ERROR) << "Invalid data size read from file descriptor. Size read: "
               << *data_size_out;
    return false;
  }

  return true;
}

}  // namespace

base::ScopedFD WriteSizeAndDataToPipe(const std::vector<uint8_t>& data) {
  size_t data_size = data.size();
  CHECK_LE(data_size, kDataSizeLimit);

  int fds[2];
  base::CreateLocalNonBlockingPipe(fds);
  base::ScopedFD read_dbus_fd(fds[0]);
  base::ScopedFD write_scoped_fd(fds[1]);

  base::WriteFileDescriptor(write_scoped_fd.get(),
                            reinterpret_cast<const char*>(&data_size),
                            sizeof(size_t));
  base::WriteFileDescriptor(write_scoped_fd.get(),
                            reinterpret_cast<const char*>(data.data()),
                            data_size);
  return read_dbus_fd;
}

bool ReadSecretFromPipe(int in_secret_fd, std::vector<uint8_t>* out_secret) {
  size_t data_size;
  if (!GetSecretDataSize(in_secret_fd, &data_size))
    return false;

  std::vector<uint8_t> buffer(data_size);
  if (!base::ReadFromFD(in_secret_fd, reinterpret_cast<char*>(buffer.data()),
                        data_size)) {
    LOG(ERROR) << "Couldn't read secret from file descriptor.";
    return false;
  }

  *out_secret = std::move(buffer);
  return true;
}

bool SaveSecretFromFileDescriptor(
    password_provider::PasswordProviderInterface* provider,
    const base::ScopedFD& in_secret_fd) {
  size_t data_size = 0;
  if (!GetSecretDataSize(in_secret_fd.get(), &data_size))
    return false;

  auto secret = password_provider::Password::CreateFromFileDescriptor(
      in_secret_fd.get(), data_size);

  if (!secret) {
    LOG(ERROR) << "Could not create secret from file descriptor.";
    return false;
  }

  if (!provider->SavePassword(*secret)) {
    LOG(ERROR) << "Could not save secret.";
    return false;
  }

  return true;
}

base::FilePath StringToSafeFilename(std::string data) {
  std::string sha256hash = crypto::SHA256HashString(data);
  return base::FilePath(base::HexEncode(sha256hash.data(), sha256hash.size()));
}

}  // namespace secret_util
}  // namespace login_manager

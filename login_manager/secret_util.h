// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SECRET_UTIL_H_
#define LOGIN_MANAGER_SECRET_UTIL_H_

#include <string>
#include <vector>

#include <base/files/scoped_file.h>
#include <base/files/file_path.h>

#include "libpasswordprovider/password_provider.h"

namespace login_manager {
namespace secret_util {

// Creates a file descriptor pointing to a pipe that contains the given data.
// The data size (of type |size_t|) will be inserted into the pipe first,
// followed by the actual data. |size_t| value representation follows the host
// byte order.
base::ScopedFD WriteSizeAndDataToPipe(const std::vector<uint8_t>& data);

// Reads secret written in |in_secret_fd| and writes it to |out_secret|.
// Secret must be preceded by |size_t| value representing its length. Returns
// 'true' if the data was successfully read, 'false' otherwise.
bool ReadSecretFromPipe(int in_secret_fd, std::vector<uint8_t>* out_secret);

// Saves secret written in |in_secret_fd| to |provider|. Secret must be
// preceded by |size_t| value representing its length. Returns 'true' if the
// data was successfully read, 'false' otherwise.
bool SaveSecretFromFileDescriptor(
    password_provider::PasswordProviderInterface* provider,
    const base::ScopedFD& in_secret_fd);

// Gets a SHA256 hash of the given data and returns its hexadicimal
// representation. This is used to generate a unique string that is safe to use
// as a filename.
base::FilePath StringToSafeFilename(std::string data);

}  // namespace secret_util
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SECRET_UTIL_H_

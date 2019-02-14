// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef KERBEROS_PLATFORM_HELPER_H_
#define KERBEROS_PLATFORM_HELPER_H_

#include <string>

#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <base/optional.h>

namespace base {
class FilePath;
}

namespace kerberos {

// Reads the whole contents of the file descriptor |fd| into the returned
// string. If fd is a blocking pipe this call will block until the pipe is
// closed. Returns nullopt if the pipe could not be read or some limit was
// exceeded (see code).
base::Optional<std::string> ReadPipeToString(int fd);

}  // namespace kerberos

#endif  // KERBEROS_PLATFORM_HELPER_H_

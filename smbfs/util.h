// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBFS_UTIL_H_
#define SMBFS_UTIL_H_

#include <string>

namespace smbfs {

// Returns a string representation of the open() flags combined to produce
// |flags| (eg. "O_RDWR|O_DIRECT|O_TRUNC").
std::string OpenFlagsToString(int flags);

// Returns a string representation of the FUSE_SET_ATTR_* flags combined to
// produce |flags|.
std::string ToSetFlagsToString(int flags);

}  // namespace smbfs

#endif  // SMBFS_UTIL_H_

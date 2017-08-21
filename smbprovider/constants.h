// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SMBPROVIDER_CONSTANTS_H_
#define SMBPROVIDER_CONSTANTS_H_

#include <stddef.h> /* for size_t */

namespace smbprovider {

// Buffer size used for reading a directory.
constexpr size_t kBufferSize = 1024 * 32;

// Entries returned by smbc_getdents() that we ignore.
extern const char kEntryParent[];
extern const char kEntrySelf[];

}  // namespace smbprovider

#endif  // SMBPROVIDER_CONSTANTS_H_

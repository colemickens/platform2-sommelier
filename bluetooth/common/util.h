// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLUETOOTH_COMMON_UTIL_H_
#define BLUETOOTH_COMMON_UTIL_H_

#include <string>

namespace bluetooth {
// Returns whether LE splitter is enabled based on config in /var/lib/bluetooth.
bool IsBleSplitterEnabled();
}  // namespace bluetooth

#endif  // BLUETOOTH_COMMON_UTIL_H_

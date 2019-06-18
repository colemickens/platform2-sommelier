// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/util.h"

#include <base/files/file_util.h>
#include <base/strings/string_util.h>

namespace {
constexpr char kNewblueConfigFile[] = "/var/lib/bluetooth/newblue";
}  // namespace

namespace bluetooth {
// True if the kernel is configured to split LE traffic.
bool IsBleSplitterEnabled() {
  std::string content;
  // LE splitter is enabled iff /var/lib/bluetooth/newblue starts with "1".
  if (base::ReadFileToString(base::FilePath(kNewblueConfigFile), &content)) {
    base::TrimWhitespaceASCII(content, base::TRIM_TRAILING, &content);
    if (content == "1")
      return true;
  }

  // Current LE splitter default = disabled.
  return false;
}
}  // namespace bluetooth

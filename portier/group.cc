// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/group.h"

#include <algorithm>

#include <base/strings/string_util.h>

namespace portier {

using std::string;

bool IsValidGroupName(const string& group_name) {
  for (char c : group_name) {
    if (!base::IsAsciiAlpha(c) && !base::IsAsciiDigit(c) && c != '_' &&
        c != '-') {
      return false;
    }
  }
  return group_name.size() > 0;
}

}  // namespace portier

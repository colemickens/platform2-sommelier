// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMOS_WIDE_PROFILING_DSO_H_
#define CHROMIUMOS_WIDE_PROFILING_DSO_H_

#include "chromiumos-wide-profiling/compat/string.h"

namespace quipper {

void InitializeLibelf();
bool ReadElfBuildId(string filename, string* buildid);

}  // namespace quipper

#endif  // CHROMIUMOS_WIDE_PROFILING_DSO_H_

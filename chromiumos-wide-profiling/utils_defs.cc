// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "utils.h"

namespace {

const char kPerfDataInputPath[] = "testdata/";

}  // namespace

namespace quipper {

extern const char kPerfPath[] = "/usr/sbin/perf";

string GetTestInputFilePath(const string& filename) {
  return string(kPerfDataInputPath) + filename;
}

}  // namespace quipper

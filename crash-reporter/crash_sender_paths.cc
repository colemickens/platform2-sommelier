// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_sender_paths.h"

namespace paths {

namespace {

// The path prefix that'll be used for testing.
const base::FilePath* g_test_prefix;

}  // namespace

void SetPrefixForTesting(const base::FilePath& prefix) {
  if (g_test_prefix) {
    delete g_test_prefix;
    g_test_prefix = nullptr;
  }
  if (!prefix.empty())
    g_test_prefix = new base::FilePath(prefix);
}

base::FilePath Get(base::StringPiece file_path) {
  if (g_test_prefix) {
    if (file_path.starts_with("/"))
      file_path.remove_prefix(1);
    return g_test_prefix->Append(file_path);
  }
  return base::FilePath(file_path);
}

}  // namespace paths

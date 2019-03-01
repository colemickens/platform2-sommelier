// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <brillo/test_helpers.h>

// TODO(crbug.com/937531): vpn-manager unit tests require SetUpTests() from
// libbrillo and thus should reuse libbrillo/testrunner.cc. Replace this file
// once crbug.com/937531 is resolved.

int main(int argc, char** argv) {
  SetUpTests(&argc, argv, true);
  return RUN_ALL_TESTS();
}

// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub implementation of kinit. Does not talk to server, but simply returns
// fixed responses to predefined input.

#include "authpolicy/stub_common.h"

int main(int argc, char* argv[]) {
  return authpolicy::kExitCodeOk;
}

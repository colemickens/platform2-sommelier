// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/newblue.h"
#include <newblue/uhid.h>

#include <stddef.h>
#include <stdint.h>

#include "bluetooth/newblued/util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t dataSize) {
  bool tempBool{false};
  parseReportDescriptorForTest(data, dataSize, &tempBool);
  return 0;
}

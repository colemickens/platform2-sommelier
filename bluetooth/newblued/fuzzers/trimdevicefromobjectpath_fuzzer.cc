// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include "bluetooth/newblued/util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t dataSize) {
  FuzzedDataProvider provider(data, dataSize);
  auto tempString = provider.ConsumeRemainingBytesAsString();
  bluetooth::TrimDeviceFromObjectPath(&tempString);
  return 0;
}

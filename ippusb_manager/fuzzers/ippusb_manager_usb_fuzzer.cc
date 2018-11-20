// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ippusb_manager/usb.h"

#include <stddef.h>
#include <stdint.h>

#include "base/test/fuzzed_data_provider.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);

  const std::string usb_info = data_provider.ConsumeRandomLengthString(size);
  uint16_t vid;
  uint16_t pid;
  ippusb_manager::GetUsbInfo(usb_info, &vid, &pid);

  return 0;
}

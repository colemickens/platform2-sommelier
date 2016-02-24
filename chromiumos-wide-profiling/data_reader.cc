// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromiumos-wide-profiling/data_reader.h"

#include "base/logging.h"

#include "chromiumos-wide-profiling/utils.h"

namespace quipper {

bool DataReader::ReadDataValue(const size_t size, const string& value_name,
                               void* dest) {
  if (ReadData(size, dest))
    return true;
  LOG(ERROR)  << "Unable to read " << value_name << ". Requested " << size
               << " bytes, " << size_ - Tell() << " bytes remaining.";
  return false;
}

bool DataReader::ReadStringWithSizeFromData(string* dest) {
  uint32_t len = 0;
  if (!ReadUint32(&len)) {
    LOG(ERROR) << "Could not read string length from data.";
    return false;
  }

  if (!ReadString(len, dest)) {
    LOG(ERROR) << "Failed to read string from data. len: " << len;
    return false;
  }
  return true;
}

}  // namespace quipper

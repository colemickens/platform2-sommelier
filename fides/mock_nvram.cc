// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/mock_nvram.h"

namespace fides {

MockNVRam::MockNVRam() {}

MockNVRam::~MockNVRam() {}

MockNVRam::Space* MockNVRam::GetSpace(uint32_t index) {
  return &spaces_[index];
}

void MockNVRam::DeleteSpace(uint32_t index) {
  spaces_.erase(index);
}

NVRam::Status MockNVRam::IsSpaceLocked(uint32_t index,
                                       bool* locked_for_reading,
                                       bool* locked_for_writing) const {
  auto entry = spaces_.find(index);
  if (entry == spaces_.end())
    return Status::kInvalidParameter;

  *locked_for_reading = entry->second.locked_for_reading_;
  *locked_for_writing = entry->second.locked_for_writing_;
  return Status::kSuccess;
}

NVRam::Status MockNVRam::ReadSpace(uint32_t index,
                                   std::vector<uint8_t>* data) const {
  auto entry = spaces_.find(index);
  if (entry == spaces_.end())
    return Status::kInvalidParameter;

  *data = entry->second.data_;
  return Status::kSuccess;
}

}  // namespace fides

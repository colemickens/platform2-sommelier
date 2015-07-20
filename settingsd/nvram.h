// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_NVRAM_H_
#define SETTINGSD_NVRAM_H_

#include <stdint.h>
#include <vector>

#include <base/macros.h>

namespace settingsd {

// An interface for accessing access-protected system NVRAM.
class NVRam {
 public:
  virtual ~NVRam() = default;

  // Status of an operation.
  enum class Status {
    kSuccess,
    kInternalError,
    kAccessDenied,
    kInvalidParameter,
  };

  // Check whether a given NVRam index is locked. Returns kInvalidParameter if
  // the specified space is not defined.
  virtual Status IsSpaceLocked(uint32_t index,
                               bool* locked_for_reading,
                               bool* locked_for_writing) const = 0;

  // Reads the specified space. Returns kInvalidParameter if the specified space
  // is not defined.
  virtual Status ReadSpace(uint32_t index,
                           std::vector<uint8_t>* data) const = 0;

 protected:
  NVRam() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(NVRam);
};

}  // namespace settingsd

#endif  // SETTINGSD_NVRAM_H_

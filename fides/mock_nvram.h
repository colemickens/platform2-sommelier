// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_MOCK_NVRAM_H_
#define FIDES_MOCK_NVRAM_H_

#include "fides/nvram.h"

#include <stdint.h>
#include <unordered_map>
#include <vector>

#include <base/macros.h>

namespace fides {

// A mock NVRam implementation that lets the test define NVRam spaces
// arbitrarily.
class MockNVRam : public NVRam {
 public:
  struct Space {
    bool locked_for_reading_;
    bool locked_for_writing_;
    std::vector<uint8_t> data_;
  };

  MockNVRam();
  ~MockNVRam() override;

  Space* GetSpace(uint32_t index);
  void DeleteSpace(uint32_t index);

  // NVRam:
  Status IsSpaceLocked(uint32_t index,
                       bool* locked_for_reading,
                       bool* locked_for_writing) const override;
  Status ReadSpace(uint32_t index, std::vector<uint8_t>* data) const override;

 private:
  std::unordered_map<uint32_t, Space> spaces_;

  DISALLOW_COPY_AND_ASSIGN(MockNVRam);
};

}  // namespace fides

#endif  // FIDES_MOCK_NVRAM_H_

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "smbprovider/sequential_id_map.h"

namespace smbprovider {

class SequentialIdMapTest : public testing::Test {
 public:
  SequentialIdMapTest() = default;
  ~SequentialIdMapTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(SequentialIdMapTest);
};

}  // namespace smbprovider

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "smbprovider/metadata_cache.h"

namespace smbprovider {

class MetadataCacheTest : public testing::Test {
 public:
  MetadataCacheTest() = default;

  ~MetadataCacheTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(MetadataCacheTest);
};

TEST_F(MetadataCacheTest, TestDefaultConstruction) {
  // TODO(zentaro): Follow up CL adds tests for functionality.
  MetadataCache cache;
}

}  // namespace smbprovider

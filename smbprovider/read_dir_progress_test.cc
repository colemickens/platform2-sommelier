// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/test/simple_test_tick_clock.h>
#include <gtest/gtest.h>

#include "smbprovider/constants.h"
#include "smbprovider/fake_samba_interface.h"
#include "smbprovider/iterator/caching_iterator.h"
#include "smbprovider/metadata_cache.h"
#include "smbprovider/read_dir_progress.h"

namespace smbprovider {

class ReadDirProgressTest : public testing::Test {
 public:
  ReadDirProgressTest()
      : fake_samba_(std::make_unique<FakeSambaInterface>()),
        fake_tick_clock_(std::make_unique<base::SimpleTestTickClock>()) {
    cache_ = std::make_unique<MetadataCache>(
        fake_tick_clock_.get(),
        base::TimeDelta::FromMicroseconds(kMetadataCacheLifetimeMicroseconds),
        MetadataCache::Mode::kDisabled);
  }
  ~ReadDirProgressTest() override = default;

 protected:
  std::unique_ptr<FakeSambaInterface> fake_samba_;
  std::unique_ptr<base::TickClock> fake_tick_clock_;
  std::unique_ptr<MetadataCache> cache_;

  DISALLOW_COPY_AND_ASSIGN(ReadDirProgressTest);
};

}  // namespace smbprovider

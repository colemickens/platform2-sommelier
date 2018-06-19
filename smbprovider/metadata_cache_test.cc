// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <gtest/gtest.h>

#include "smbprovider/metadata_cache.h"

namespace smbprovider {

namespace {
bool AreEntriesEqual(const DirectoryEntry& lhs, const DirectoryEntry& rhs) {
  return lhs.name == rhs.name && lhs.full_path == rhs.full_path &&
         lhs.size == rhs.size &&
         lhs.last_modified_time == rhs.last_modified_time &&
         lhs.is_directory == rhs.is_directory;
}
}  // namespace

class MetadataCacheTest : public testing::Test {
 public:
  MetadataCacheTest() { cache_ = std::make_unique<MetadataCache>(); }

  ~MetadataCacheTest() override = default;

 protected:
  std::unique_ptr<MetadataCache> cache_;

  DISALLOW_COPY_AND_ASSIGN(MetadataCacheTest);
};

TEST_F(MetadataCacheTest, FindOnEmptyCache) {
  DirectoryEntry found_entry;
  EXPECT_FALSE(cache_->FindEntry("smb://server/share/not/found", &found_entry));
}

TEST_F(MetadataCacheTest, AddAndFindEntry) {
  const std::string name = "file";
  const std::string full_path = "smb://server/share/dir/" + name;

  DirectoryEntry found_entry;
  EXPECT_FALSE(cache_->FindEntry(full_path, &found_entry));

  const int64_t expected_size = 1234;
  const int64_t expected_date = 9999999;
  const DirectoryEntry expected_entry(false /* is_directory */, name, full_path,
                                      expected_size, expected_date);
  cache_->AddEntry(expected_entry);
  EXPECT_TRUE(cache_->FindEntry(full_path, &found_entry));
  EXPECT_TRUE(AreEntriesEqual(expected_entry, found_entry));

  // Verify it can be found again.
  DirectoryEntry found_entry2;
  EXPECT_TRUE(cache_->FindEntry(full_path, &found_entry2));
  EXPECT_TRUE(AreEntriesEqual(expected_entry, found_entry2));
}

}  // namespace smbprovider

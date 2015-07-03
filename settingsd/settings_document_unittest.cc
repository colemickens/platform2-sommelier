// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/settings_document.h"

#include <gtest/gtest.h>

#include "settingsd/mock_settings_document.h"
#include "settingsd/test_helpers.h"

namespace settingsd {

class SettingsDocumentTestBase : public testing::Test {
 public:
  SettingsDocumentTestBase() : A(version_stamp), B(version_stamp) {}

 protected:
  VersionStamp version_stamp;
  MockSettingsDocument A;
  MockSettingsDocument B;
};

TEST_F(SettingsDocumentTestBase, OverlapParallelKey) {
  A.SetEntry(Key("A"), MakeIntValue(1));
  B.SetEntry(Key("B"), MakeIntValue(1));
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapParallelDeletion) {
  A.SetDeletion(Key("A"));
  B.SetDeletion(Key("B"));
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapSameKey) {
  A.SetEntry(Key("A"), MakeIntValue(1));
  B.SetEntry(Key("A"), MakeIntValue(1));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapSameDeletion) {
  A.SetDeletion(Key("A"));
  B.SetDeletion(Key("A"));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapSameDeletionAndKey) {
  A.SetDeletion(Key("A"));
  B.SetEntry(Key("A"), MakeIntValue(1));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapKeyAndParentKey) {
  A.SetEntry(Key("A"), MakeIntValue(1));
  B.SetEntry(Key("A.B"), MakeIntValue(1));
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapKeyAndParentDeletion) {
  A.SetDeletion(Key("A"));
  B.SetEntry(Key("A.B"), MakeIntValue(1));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapDeletionAndParentDeletion) {
  A.SetDeletion(Key("A"));
  B.SetDeletion(Key("A.B"));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapDeletionAndParentKey) {
  A.SetDeletion(Key("A.B"));
  B.SetEntry(Key("A"), MakeIntValue(1));
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

}  // namespace settingsd

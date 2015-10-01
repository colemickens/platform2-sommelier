// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/settings_document.h"

#include <gtest/gtest.h>

#include "fides/mock_settings_document.h"
#include "fides/test_helpers.h"

namespace fides {

namespace {

const std::string kSourceId = "source";

}  // namespace

class SettingsDocumentTestBase : public testing::Test {
 public:
  SettingsDocumentTestBase() : A(version_stamp), B(version_stamp) {}

 protected:
  VersionStamp version_stamp;
  MockSettingsDocument A;
  MockSettingsDocument B;
};

TEST_F(SettingsDocumentTestBase, OverlapParallelKey) {
  A.SetKey(Key("A"), "1");
  B.SetKey(Key("B"), "1");
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapParallelDeletion) {
  A.SetDeletion(Key("A"));
  B.SetDeletion(Key("B"));
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapSameKey) {
  A.SetKey(Key("A"), "1");
  B.SetKey(Key("A"), "1");
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapSameDeletion) {
  A.SetDeletion(Key("A"));
  B.SetDeletion(Key("A"));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapSameDeletionAndKey) {
  A.SetDeletion(Key("A"));
  B.SetKey(Key("A"), "1");
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapKeyAndParentKey) {
  A.SetKey(Key("A"), "1");
  B.SetKey(Key("A.B"), "1");
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapKeyAndParentDeletion) {
  A.SetDeletion(Key("A"));
  B.SetKey(Key("A.B"), "1");
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapDeletionAndParentDeletion) {
  A.SetDeletion(Key("A"));
  B.SetDeletion(Key("A.B"));
  EXPECT_TRUE(SettingsDocument::HasOverlap(A, B));
}

TEST_F(SettingsDocumentTestBase, OverlapDeletionAndParentKey) {
  A.SetDeletion(Key("A.B"));
  B.SetKey(Key("A"), "1");
  EXPECT_FALSE(SettingsDocument::HasOverlap(A, B));
}

}  // namespace fides

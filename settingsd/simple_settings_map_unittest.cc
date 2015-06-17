// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/simple_settings_map.h"

#include <base/values.h>
#include <gtest/gtest.h>
#include <memory>

#include "settingsd/mock_settings_document.h"

namespace settingsd {

TEST(SimpleSettingsMapTest, Basic) {
  // Prepare document for writer A.
  VersionStamp version_stamp_A;
  version_stamp_A.Set("A", 1);
  version_stamp_A.Set("B", 1);
  std::unique_ptr<MockSettingsDocument> document_A(
      new MockSettingsDocument(version_stamp_A));

  // Prepare Document for writer B.
  VersionStamp version_stamp_B;
  version_stamp_B.Set("A", 2);
  version_stamp_B.Set("B", 1);
  std::unique_ptr<MockSettingsDocument> document_B(
      new MockSettingsDocument(version_stamp_B));

  // document A
  document_A->SetEntry(
      "A.B.C", std::unique_ptr<base::Value>(new base::FundamentalValue(1)));

  // document B
  document_B->SetEntry(
      "A.B", std::unique_ptr<base::Value>(new base::FundamentalValue(2)));
  document_B->SetEntry(
      "A.B.", std::unique_ptr<base::Value>(base::Value::CreateNullValue()));

  SimpleSettingsMap settings_map;
  settings_map.InsertDocument(std::move(document_A));
  settings_map.InsertDocument(std::move(document_B));

  std::set<std::string> expected_prefixes;
  expected_prefixes.insert("A.B");
  expected_prefixes.insert("A.B.");
  std::set<std::string> child_prefixes =
      settings_map.GetActiveChildPrefixes(kRootPrefix);

  EXPECT_EQ(expected_prefixes, child_prefixes);
}

}  // namespace settingsd

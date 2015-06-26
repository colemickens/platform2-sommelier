// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/source.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/values.h>
#include <gtest/gtest.h>

#include "settingsd/identifier_utils.h"
#include "settingsd/mock_settings_service.h"
#include "settingsd/settings_keys.h"
#include "settingsd/test_helpers.h"

namespace settingsd {

// Test source constants.
const char kSource1[] = "source1";

const char kName1[] = "Name1";

const char kSourceType[] = "dummy_source_type";

class TestSourceDelegate : public SourceDelegate {
 public:
  TestSourceDelegate() = default;
  ~TestSourceDelegate() override = default;

  // SourceDelegate:
  bool ValidateVersionComponentBlob(
      const VersionComponentBlob& blob) const override {
    return true;
  }
  bool ValidateSettingsBlob(const SettingsBlob& blob) const override {
    return true;
  }
};

class SourceTest : public testing::Test {
 public:
  SourceTest() {
    settings_.SetValue(
        MakeSourceKey(kSource1).Extend({keys::sources::kName}),
        MakeStringValue(kName1));
    settings_.SetValue(
        MakeSourceKey(kSource1).Extend({keys::sources::kStatus}),
        MakeStringValue(SettingStatusToString(kSettingStatusActive)));
    settings_.SetValue(
        MakeSourceKey(kSource1).Extend({keys::sources::kType}),
        MakeStringValue(kSourceType));
  }

  SourceDelegateFactoryFunction GetSourceDelegateFactoryFunction() {
    return std::ref(*this);
  }

  std::unique_ptr<SourceDelegate> operator()(
      const std::string& source_id,
      const SettingsService& settings) {
    created_delegates_[source_id] = new TestSourceDelegate();
    return std::unique_ptr<SourceDelegate>(created_delegates_[source_id]);
  }

  MockSettingsService settings_;
  std::map<std::string, SourceDelegate*> created_delegates_;
};

TEST_F(SourceTest, Update) {
  Source source(kSource1);

  // Check default source after creation.
  EXPECT_EQ(kSource1, source.id());
  EXPECT_TRUE(source.name().empty());
  EXPECT_EQ(kSettingStatusInvalid, source.status());
  EXPECT_TRUE(source.delegate());

  // Update the source from settings.
  source.Update(GetSourceDelegateFactoryFunction(), settings_);

  EXPECT_EQ(kSource1, source.id());
  EXPECT_EQ(kName1, source.name());
  EXPECT_EQ(kSettingStatusActive, source.status());
  EXPECT_TRUE(source.delegate());
}

TEST_F(SourceTest, Delegates) {
  Source source(kSource1);
  ASSERT_TRUE(source.delegate());
  EXPECT_EQ(created_delegates_.end(), created_delegates_.find(kSource1));

  source.Update(GetSourceDelegateFactoryFunction(), settings_);
  ASSERT_TRUE(source.delegate());
  EXPECT_NE(created_delegates_.end(), created_delegates_.find(kSource1));
}

}  // namespace settingsd

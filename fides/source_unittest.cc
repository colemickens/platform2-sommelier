// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/source.h"

#include <map>
#include <memory>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "fides/identifier_utils.h"
#include "fides/mock_settings_document.h"
#include "fides/mock_settings_service.h"
#include "fides/settings_keys.h"
#include "fides/test_helpers.h"
#include "fides/version_stamp.h"

namespace fides {

// Test source constants.
const char kSource0[] = "source0";
const char kSource1[] = "source1";
const char kSource2[] = "source2";

const char kName1[] = "Name1";

const char kSourceType[] = "dummy_source_type";

class TestSourceDelegate : public SourceDelegate {
 public:
  TestSourceDelegate() = default;
  ~TestSourceDelegate() override = default;

  // SourceDelegate:
  bool ValidateVersionComponent(
      const LockedVersionComponent& component) const override {
    return true;
  }
  bool ValidateContainer(
      const LockedSettingsContainer& container) const override {
    return true;
  }
};

class SourceTest : public testing::Test {
 public:
  SourceTest() {
    settings_.SetValue(
        MakeSourceKey(kSource1).Extend({keys::sources::kName}),
        kName1);
    settings_.SetValue(
        MakeSourceKey(kSource1).Extend({keys::sources::kStatus}),
        SettingStatusToString(kSettingStatusActive));
    settings_.SetValue(
        MakeSourceKey(kSource1).Extend({keys::sources::kType}),
        kSourceType);
    settings_.SetValue(
        MakeSourceKey(kSource2).Extend({keys::sources::kStatus}),
        SettingStatusToString(kSettingStatusWithdrawn));

    // Access rules for some random keys.
    SetAccessRule(kSource1, Key("A.B"), kSettingStatusActive);
    SetAccessRule(kSource1, Key("B"), kSettingStatusWithdrawn);
    SetAccessRule(kSource1, Key("C"), kSettingStatusActive);
    SetAccessRule(kSource1, Key("C.D.E"), kSettingStatusInvalid);
    SetAccessRule(kSource1, Key("C.D.E.F"), kSettingStatusActive);
    SetAccessRule(kSource1, Key("D"), kSettingStatusActive);

    // Trust config access rules.
    SetAccessRule(kSource1, MakeSourceKey(kSource0), kSettingStatusActive);
    SetAccessRule(kSource1,
                  MakeSourceKey(kSource1).Extend({keys::sources::kStatus}),
                  kSettingStatusActive);
    SetAccessRule(kSource1, MakeSourceKey(kSource2), kSettingStatusActive);

    SetAccessRule(kSource2, Key(), kSettingStatusActive);
  }

  SourceDelegateFactoryFunction GetSourceDelegateFactoryFunction() {
    return std::ref(*this);
  }

  void SetAccessRule(const std::string& source,
                     const Key& prefix,
                     SettingStatus status) {
    settings_.SetValue(
        MakeSourceKey(source).Extend({keys::sources::kAccess}).Append(prefix),
        SettingStatusToString(status));
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

TEST_F(SourceTest, CheckAccess) {
  Source source(kSource1);
  MockSettingsDocument doc((VersionStamp()));

  // No access - the source isn't marked as active.
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  // Update the source from settings.
  source.Update(GetSourceDelegateFactoryFunction(), settings_);

  // Check access rules for keys work as expected.
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusActive));

  doc.SetKey(Key(), "1");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusActive));

  doc.ClearKeys();
  doc.SetKey(Key("A.B"), "0");
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusActive));
  doc.SetKey(Key("A.B.C"), "0");
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusActive));

  doc.SetKey(Key("B"), "0");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusActive));
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.SetKey(Key("C"), "0");
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.SetKey(Key("C.D.E.suffix"), "0");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.ClearKey(Key("C.D.E.suffix"));
  doc.SetKey(Key("C.D.E.F"), "0");
  doc.SetKey(Key("C.D.E.F.G"), "0");
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.SetKey(Key("D.suffix"), "0");
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.SetKey(Key("E"), "0");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  // Check access rules for deletions work as expected.
  doc.ClearKeys();
  doc.SetDeletion(Key("A.B"));
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusActive));

  doc.SetDeletion(Key("B"));
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusActive));
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.ClearDeletions();
  doc.SetDeletion(Key("A"));
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.ClearDeletions();
  doc.SetDeletion(Key("C"));
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.ClearDeletions();
  doc.SetDeletion(Key());
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  // A source in withdrawn state doesn't pass access checks for active state.
  Source source2(kSource2);
  source2.Update(GetSourceDelegateFactoryFunction(), settings_);
  doc.ClearDeletions();
  doc.SetKey(Key("A"), "0");
  EXPECT_FALSE(source2.CheckAccess(&doc, kSettingStatusActive));
  EXPECT_TRUE(source2.CheckAccess(&doc, kSettingStatusWithdrawn));
}

TEST_F(SourceTest, CheckAccessTrustConfig) {
  Source source(kSource1);
  source.Update(GetSourceDelegateFactoryFunction(), settings_);

  MockSettingsDocument doc((VersionStamp()));

  // Access to higher-precedence sources is denied even though there's an
  // explicit access rule.
  doc.SetKey(MakeSourceKey(kSource0).Extend({keys::sources::kStatus}),
             "0");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  // Access to own trust config is denied.
  doc.ClearKeys();
  doc.SetKey(MakeSourceKey(kSource1).Extend({keys::sources::kStatus}),
             "0");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  doc.ClearKeys();
  doc.SetKey(MakeSourceKey(kSource1), "0");
  EXPECT_FALSE(source.CheckAccess(&doc, kSettingStatusWithdrawn));

  // Access to whitelisted lower-precedence source is allowed.
  doc.ClearKeys();
  doc.SetKey(MakeSourceKey(kSource2), "0");
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusActive));

  // Deletions of lower-precedence sources are allowed.
  doc.ClearKeys();
  doc.SetDeletion(MakeSourceKey(kSource2));
  EXPECT_TRUE(source.CheckAccess(&doc, kSettingStatusActive));

  // Root deletions are disallowed because they include the off-bounds trust
  // section, even if the root is explicitly whitelisted.
  Source source2(kSource2);
  source2.Update(GetSourceDelegateFactoryFunction(), settings_);
  doc.ClearDeletions();
  doc.SetKey(Key("A"), "0");
  EXPECT_TRUE(source2.CheckAccess(&doc, kSettingStatusWithdrawn));
  doc.SetDeletion(Key());
  EXPECT_FALSE(source2.CheckAccess(&doc, kSettingStatusWithdrawn));
}

TEST_F(SourceTest, Delegates) {
  Source source(kSource1);
  ASSERT_TRUE(source.delegate());
  EXPECT_EQ(created_delegates_.end(), created_delegates_.find(kSource1));

  source.Update(GetSourceDelegateFactoryFunction(), settings_);
  ASSERT_TRUE(source.delegate());
  EXPECT_NE(created_delegates_.end(), created_delegates_.find(kSource1));
}

}  // namespace fides

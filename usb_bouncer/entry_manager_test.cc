// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gtest/gtest.h>

#include "usb_bouncer/entry_manager.h"
#include "usb_bouncer/entry_manager_test_util.h"
#include "usb_bouncer/util.h"

namespace usb_bouncer {

class EntryManagerTest : public testing::Test {
 public:
  void GarbageCollectTest(bool user_present) {
    util_.RefreshDB(user_present /*include_user_db*/, true /*new_db*/);

    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd,
                                        kDefaultDevpath));
    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kRemove,
                                        kDefaultDevpath));

    EXPECT_EQ(util_.GarbageCollectInternal(true /*global_only*/), 0);
    EXPECT_TRUE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_TRUE(util_.UserDBContainsEntry(kDefaultRule));
    }

    EXPECT_TRUE(util_.Get()->GarbageCollect());
    EXPECT_TRUE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_TRUE(util_.UserDBContainsEntry(kDefaultRule));
    }

    util_.ExpireEntry(user_present, kDefaultDevpath, kDefaultRule);

    EXPECT_EQ(util_.GarbageCollectInternal(true /*global_only*/), 1);
    EXPECT_FALSE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_TRUE(util_.UserDBContainsEntry(kDefaultRule));
    }

    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd,
                                        kDefaultDevpath));
    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kRemove,
                                        kDefaultDevpath));
    util_.ExpireEntry(user_present, kDefaultDevpath, kDefaultRule);

    EXPECT_TRUE(util_.Get()->GarbageCollect());
    EXPECT_FALSE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_FALSE(util_.UserDBContainsEntry(kDefaultRule));
    }
  }

  void GenerateRulesTest(bool user_present) {
    util_.RefreshDB(user_present /*include_user_db*/, true /*new_db*/);

    EXPECT_FALSE(util_.Get()->GenerateRules().empty());

    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd,
                                        kDefaultDevpath));

    std::string rules = util_.Get()->GenerateRules();
    EXPECT_FALSE(rules.empty());
    EXPECT_NE(rules.find(kDefaultRule, 0), std::string::npos);
  }

  void HandleUdevTest(bool user_present) {
    util_.RefreshDB(user_present /*include_user_db*/, true /*new_db*/);

    EXPECT_FALSE(util_.GlobalDBContainsEntry(kDefaultDevpath, kDefaultRule));
    EXPECT_FALSE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_FALSE(util_.UserDBContainsEntry(kDefaultRule));
    }
    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd,
                                        kDefaultDevpath));
    EXPECT_TRUE(util_.GlobalDBContainsEntry(kDefaultDevpath, kDefaultRule));
    EXPECT_FALSE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_TRUE(util_.UserDBContainsEntry(kDefaultRule));
    }

    EXPECT_FALSE(util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd, ""));
    EXPECT_FALSE(util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd,
                                         kUsbguardPolicyDir));

    EXPECT_TRUE(util_.Get()->HandleUdev(EntryManager::UdevAction::kRemove,
                                        kDefaultDevpath));
    EXPECT_FALSE(util_.GlobalDBContainsEntry(kDefaultDevpath, kDefaultRule));
    EXPECT_TRUE(util_.GlobalTrashContainsEntry(kDefaultDevpath, kDefaultRule));
    if (user_present) {
      EXPECT_TRUE(util_.UserDBContainsEntry(kDefaultRule));
    }

    EXPECT_FALSE(
        util_.Get()->HandleUdev(EntryManager::UdevAction::kRemove, ""));
  }

 protected:
  EntryManagerTestUtil util_;
};

TEST_F(EntryManagerTest, GarbageCollect_NoUser) {
  GarbageCollectTest(false /*user_present*/);
}

TEST_F(EntryManagerTest, GarbageCollect_WithUser) {
  GarbageCollectTest(true /*user_present*/);
}

TEST_F(EntryManagerTest, GenerateRules_NoUser) {
  GenerateRulesTest(false /*user_present*/);
}

TEST_F(EntryManagerTest, GenerateRules_WithUser) {
  GenerateRulesTest(true /*user_present*/);
}

TEST_F(EntryManagerTest, HandleUdev_NoUser) {
  HandleUdevTest(false /*user_present*/);
}

TEST_F(EntryManagerTest, HandleUdev_WithUser) {
  HandleUdevTest(true /*user_present*/);
}

TEST_F(EntryManagerTest, HandleUserLogin_NoUser) {
  util_.RefreshDB(false /*include_user_db*/, true /*new_db*/);

  EXPECT_FALSE(util_.Get()->HandleUserLogin());
}

TEST_F(EntryManagerTest, HandleUserLogin_WithUser) {
  util_.RefreshDB(false /*include_user_db*/, true /*new_db*/);

  EXPECT_TRUE(
      util_.Get()->HandleUdev(EntryManager::UdevAction::kAdd, kDefaultDevpath));

  util_.RefreshDB(true /*include_user_db*/, false /*new_db*/);

  EXPECT_TRUE(util_.GlobalDBContainsEntry(kDefaultDevpath, kDefaultRule));
  EXPECT_FALSE(util_.UserDBContainsEntry(kDefaultRule));

  EXPECT_TRUE(util_.Get()->HandleUserLogin());
  EXPECT_TRUE(util_.UserDBContainsEntry(kDefaultRule));
}

}  // namespace usb_bouncer

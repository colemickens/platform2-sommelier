// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cros-disks/fuse_mount_manager.h"

#include <string>
#include <utility>
#include <vector>

#include <base/strings/string_util.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cros-disks/fuse_helper.h"
#include "cros-disks/metrics.h"
#include "cros-disks/mount_options.h"
#include "cros-disks/platform.h"

using std::set;
using std::string;
using std::vector;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::Return;
using testing::SetArgPointee;
using testing::UnorderedElementsAre;
using testing::_;

namespace cros_disks {

namespace {

const char kMountRoot[] = "/mntroot";
const char kNoType[] = "";
const char kSomeSource[] = "/dev/sdz";
const char kSomeMountpoint[] = "/mnt";

// Mock Platform implementation for testing.
class MockPlatform : public Platform {
 public:
  MockPlatform() = default;

  MOCK_CONST_METHOD1(Unmount, bool(const string& path));
};

// Mock implementation of FUSEHelper.
class MockHelper : public FUSEHelper {
 public:
  MockHelper(const string& tag, const Platform* platform)
      : FUSEHelper(tag, platform, "/sbin/" + tag, "fuse-" + tag) {}

  MOCK_CONST_METHOD1(CanMount, bool(const string& src));
  MOCK_CONST_METHOD1(GetTargetSuffix, string(const string& src));
  MOCK_CONST_METHOD3(Mount,
                     MountErrorType(const string& src,
                                    const string& dst,
                                    const vector<string>& opts));
};

}  // namespace

class FUSEMountManagerTest : public ::testing::Test {
 public:
  FUSEMountManagerTest()
      : manager_(kMountRoot, &platform_, &metrics_),
        foo_(new MockHelper("foo", &platform_)),
        bar_(new MockHelper("bar", &platform_)),
        baz_(new MockHelper("baz", &platform_)) {
    ON_CALL(platform_, Unmount(_)).WillByDefault(Return(false));
  }

 protected:
  void RegisterHelper(std::unique_ptr<FUSEHelper> helper) {
    manager_.RegisterHelper(std::move(helper));
  }

  MountErrorType DoMount(const string& type, const string& src) {
    MountOptions mount_options;
    return manager_.DoMount(src, type, {}, kSomeMountpoint, &mount_options);
  }

  Metrics metrics_;
  MockPlatform platform_;
  FUSEMountManager manager_;
  std::unique_ptr<MockHelper> foo_;
  std::unique_ptr<MockHelper> bar_;
  std::unique_ptr<MockHelper> baz_;
};

// Verifies that CanMount returns false when there are no handlers registered.
TEST_F(FUSEMountManagerTest, CanMount_NoHandlers) {
  EXPECT_FALSE(manager_.CanMount(kSomeSource));
}

// Verifies that CanMount returns false when known helpers can't handle that.
TEST_F(FUSEMountManagerTest, CanMount_NotHandled) {
  EXPECT_CALL(*foo_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*bar_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*baz_, CanMount(_)).WillOnce(Return(false));
  RegisterHelper(std::move(foo_));
  RegisterHelper(std::move(bar_));
  RegisterHelper(std::move(baz_));
  EXPECT_FALSE(manager_.CanMount(kSomeSource));
}

// Verify that CanMount returns true when there is a helper that can handle
// this source.
TEST_F(FUSEMountManagerTest, CanMount) {
  EXPECT_CALL(*foo_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*bar_, CanMount(_)).WillOnce(Return(true));
  EXPECT_CALL(*baz_, CanMount(_)).Times(0);
  RegisterHelper(std::move(foo_));
  RegisterHelper(std::move(bar_));
  RegisterHelper(std::move(baz_));
  EXPECT_TRUE(manager_.CanMount(kSomeSource));
}

// Verify that SuggestMountPath dispatches query for name to the correct helper.
TEST_F(FUSEMountManagerTest, SuggestMountPath) {
  EXPECT_CALL(*foo_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*foo_, GetTargetSuffix(_)).Times(0);
  EXPECT_CALL(*bar_, CanMount(_)).WillOnce(Return(true));
  EXPECT_CALL(*bar_, GetTargetSuffix(kSomeSource)).WillOnce(Return("suffix"));
  EXPECT_CALL(*baz_, CanMount(_)).Times(0);
  EXPECT_CALL(*baz_, GetTargetSuffix(_)).Times(0);
  RegisterHelper(std::move(foo_));
  RegisterHelper(std::move(bar_));
  RegisterHelper(std::move(baz_));
  EXPECT_EQ("/mntroot/suffix", manager_.SuggestMountPath(kSomeSource));
}

// Verify that DoUnmount delegates unmount directly to platform.
TEST_F(FUSEMountManagerTest, DoUnmount) {
  EXPECT_CALL(platform_, Unmount(kSomeSource)).WillOnce(Return(true));
  EXPECT_CALL(platform_, Unmount("foobar")).WillOnce(Return(false));
  EXPECT_EQ(MOUNT_ERROR_NONE, manager_.DoUnmount(kSomeSource, {}));
  EXPECT_NE(MOUNT_ERROR_NONE, manager_.DoUnmount("foobar", {}));
}

// Verify that DoMount fails when there are not helpers.
TEST_F(FUSEMountManagerTest, DoMount_NoHandlers) {
  EXPECT_EQ(MOUNT_ERROR_UNKNOWN_FILESYSTEM, DoMount(kNoType, kSomeSource));
}

// Verify that DoMount fails when helpers don't handle this source.
TEST_F(FUSEMountManagerTest, DoMount_NotHandled) {
  EXPECT_CALL(*foo_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*bar_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*baz_, CanMount(_)).WillOnce(Return(false));
  RegisterHelper(std::move(foo_));
  RegisterHelper(std::move(bar_));
  RegisterHelper(std::move(baz_));
  EXPECT_EQ(MOUNT_ERROR_UNKNOWN_FILESYSTEM, DoMount(kNoType, kSomeSource));
}

// Verify that DoMount delegates mounting to the correct helpers when
// dispatching by source description.
TEST_F(FUSEMountManagerTest, DoMount_BySource) {
  EXPECT_CALL(*foo_, CanMount(_)).WillOnce(Return(false));
  EXPECT_CALL(*bar_, CanMount(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*baz_, CanMount(_)).Times(0);
  EXPECT_CALL(*foo_, Mount(_, _, _)).Times(0);
  EXPECT_CALL(*bar_, Mount(kSomeSource, kSomeMountpoint, _))
      .WillOnce(Return(MOUNT_ERROR_NONE));
  EXPECT_CALL(*baz_, Mount(_, _, _)).Times(0);
  RegisterHelper(std::move(foo_));
  RegisterHelper(std::move(bar_));
  RegisterHelper(std::move(baz_));
  EXPECT_EQ(MOUNT_ERROR_NONE, DoMount(kNoType, kSomeSource));
}

}  // namespace cros_disks

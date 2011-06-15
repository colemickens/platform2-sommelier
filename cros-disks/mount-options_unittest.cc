// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/mount.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gtest/gtest.h>

#include "cros-disks/mount-options.h"

namespace cros_disks {

class MountOptionsTest : public ::testing::Test {
 public:
  MountOptionsTest() {}
  virtual ~MountOptionsTest() {}
  virtual void SetUp() {}
  virtual void TearDown() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MountOptionsTest);
};

TEST_F(MountOptionsTest, IsReadOnlyOptionSet) {
  MountOptions mount_options;
  std::vector<std::string> options;

  // default construction
  EXPECT_EQ(true, mount_options.IsReadOnlyOptionSet());

  // options: ro
  options.push_back("ro");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(true, mount_options.IsReadOnlyOptionSet());

  // options: rw
  options.clear();
  options.push_back("rw");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(false, mount_options.IsReadOnlyOptionSet());
}

TEST_F(MountOptionsTest, SetReadOnlyOption) {
  MountOptions mount_options;
  std::vector<std::string> options;
  std::string expected_string = "ro";

  // default construction
  mount_options.SetReadOnlyOption();
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: ro
  options.push_back("ro");
  mount_options.Initialize(options, false, false, "", "");;
  mount_options.SetReadOnlyOption();
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: rw
  options.clear();
  options.push_back("rw");
  mount_options.Initialize(options, false, false, "", "");;
  mount_options.SetReadOnlyOption();
  EXPECT_EQ(expected_string, mount_options.ToString());
}

TEST_F(MountOptionsTest, ToString) {
  MountOptions mount_options;
  std::vector<std::string> options;
  std::string expected_string;

  // default construction
  expected_string = "ro";
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: ro (default)
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: ro, nodev
  expected_string = "nodev,ro";
  options.push_back("nodev");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw
  expected_string = "nodev,rw";
  options.push_back("rw");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid
  expected_string = "nodev,nosuid,rw";
  options.push_back("nosuid");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid, noexec
  expected_string = "nodev,nosuid,noexec,rw";
  options.push_back("noexec");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid, noexec, sync
  expected_string = "nodev,nosuid,noexec,sync,rw";
  options.push_back("sync");
  mount_options.Initialize(options, false, false, "", "");;
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid, noexec, sync
  // force read-only with default uid=1000, gid=1001
  // ignore user and group ID
  expected_string = "nodev,nosuid,noexec,sync,ro";
  mount_options.Initialize(options, true, false, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid, noexec, sync
  // force read-only with default uid=1000, gid=1001
  expected_string = "nodev,nosuid,noexec,sync,ro,uid=1000,gid=1001";
  mount_options.Initialize(options, true, true, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid, noexec, sync, uid=2000, gid=2001
  // force read-only with default uid=1000, gid=1001
  // ignore user and group ID
  options.push_back("uid=2000");
  options.push_back("gid=2001");
  expected_string = "nodev,nosuid,noexec,sync,ro";
  mount_options.Initialize(options, true, false, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options.ToString());

  // options: nodev, rw, nosuid, noexec, sync, uid=2000, gid=2001
  // force read-only with default uid=1000, gid=1001
  expected_string = "nodev,nosuid,noexec,sync,ro,uid=2000,gid=2001";
  mount_options.Initialize(options, true, true, "1000", "1001");
  EXPECT_EQ(expected_string, mount_options.ToString());
}

TEST_F(MountOptionsTest, ToMountFlagsAndData) {
  MountOptions mount_options;
  std::vector<std::string> options;
  unsigned long expected_flags;
  std::string expected_data;
  std::pair<unsigned long, std::string> flags_and_data;

  // default construction
  expected_flags = MS_RDONLY;
  expected_data = "";
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: ro (default)
  mount_options.Initialize(options, false, false, "", "");;
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: ro, nodev
  options.push_back("nodev");
  expected_flags = MS_RDONLY | MS_NODEV;
  mount_options.Initialize(options, false, false, "", "");;
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw
  options.push_back("rw");
  expected_flags = MS_NODEV;
  mount_options.Initialize(options, false, false, "", "");;
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid
  options.push_back("nosuid");
  expected_flags = MS_NODEV | MS_NOSUID;
  mount_options.Initialize(options, false, false, "", "");;
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec
  options.push_back("noexec");
  expected_flags = MS_NODEV | MS_NOSUID | MS_NOEXEC;
  mount_options.Initialize(options, false, false, "", "");;
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync
  options.push_back("sync");
  expected_flags = MS_NODEV | MS_NOSUID | MS_NOEXEC | MS_SYNCHRONOUS;
  mount_options.Initialize(options, false, false, "", "");;
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync
  // force read-only with default uid=1000, gid=1001
  // ignore user and group ID
  expected_flags = MS_RDONLY | MS_NODEV | MS_NOSUID | MS_NOEXEC |
    MS_SYNCHRONOUS;
  mount_options.Initialize(options, true, false, "1000", "1001");
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync
  // force read-only with default uid=1000, gid=1001
  expected_data = "uid=1000,gid=1001";
  mount_options.Initialize(options, true, true, "1000", "1001");
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync, uid=2000, gid=2001
  // force read-only with default uid=1000, gid=1001
  // ignore user and group ID
  options.push_back("uid=2000");
  options.push_back("gid=2001");
  expected_data = "";
  mount_options.Initialize(options, true, false, "1000", "1001");
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);

  // options: nodev, rw, nosuid, noexec, sync, uid=2000, gid=2001
  // force read-only with default uid=1000, gid=1001
  expected_data = "uid=2000,gid=2001";
  mount_options.Initialize(options, true, true, "1000", "1001");
  flags_and_data = mount_options.ToMountFlagsAndData();
  EXPECT_EQ(expected_flags, flags_and_data.first);
  EXPECT_EQ(expected_data, flags_and_data.second);
}

}  // namespace cros_disks

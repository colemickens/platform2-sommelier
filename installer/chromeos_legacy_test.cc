// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "installer/chromeos_legacy.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "installer/chromeos_install_config.h"

using std::string;

class LegacyTest : public ::testing::Test {};

TEST(LegacyTest, EfiGrubUpdateTest) {
  // this string is a grub file stripped down to (mostly) just what we update.
  string input =
      "unrelated line\n"
      "\n"
      "  linux /syslinux/vmlinuz.A cros_efi cros_debug "
      "root=PARTUUID=CC6F2E74-8803-7843-B674-8481EF4CF673\n"
      "  linux /syslinux/vmlinuz.B cros_efi cros_debug "
      " root=PARTUUID=5BFD65FE-0398-804A-B090-A201E022A7C6\n"
      "  linux /syslinux/vmlinuz.A cros_efi cros_debug "
      "root=/dev/dm-0 dm=\"DM verity=A\"\n"
      "  linux /syslinux/vmlinuz.B cros_efi cros_debug "
      "root=/dev/dm-0 dm=\"DM verity=B\"\n"
      "  linux (hd0,3)/boot/vmlinuz quiet console=tty2 init=/sbin/init "
      "boot=local rootwait ro noresume noswap loglevel=1 noinitrd "
      "root=/dev/sdb3 i915.modeset=1 cros_efi cros_debug\n";

  string expected =
      "unrelated line\n"
      "\n"
      "  linux /syslinux/vmlinuz.A cros_efi cros_debug "
      "root=PARTUUID=fake_root_uuid\n"
      "  linux /syslinux/vmlinuz.B cros_efi cros_debug "
      " root=PARTUUID=5BFD65FE-0398-804A-B090-A201E022A7C6\n"
      "  linux /syslinux/vmlinuz.A cros_efi cros_debug "
      "root=/dev/dm-0 dm=\"verity args\"\n"
      "  linux /syslinux/vmlinuz.B cros_efi cros_debug "
      "root=/dev/dm-0 dm=\"DM verity=B\"\n"
      "  linux (hd0,3)/boot/vmlinuz quiet console=tty2 init=/sbin/init "
      "boot=local rootwait ro noresume noswap loglevel=1 noinitrd "
      "root=/dev/sdb3 i915.modeset=1 cros_efi cros_debug\n";

  string output;
  EXPECT_TRUE(
      EfiGrubUpdate(input, "A", "fake_root_uuid", "verity args", &output));
  EXPECT_EQ(output, expected);
}

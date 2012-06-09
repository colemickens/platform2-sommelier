// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "chromeos_install_config.h"
#include "chromeos_postinst.h"
#include "inst_util.h"

using std::string;

void TestConfigureInstall(const std::string& install_dev,
                          const std::string& install_path,
                          bool expected_success,
                          const std::string& expected_slot,
                          const std::string& expected_root,
                          const std::string& expected_kernel,
                          const std::string& expected_boot) {

  InstallConfig install_config;

  BiosType expected_bios = kBiosTypeSecure;

  EXPECT_EQ(ConfigureInstall(install_dev,
                             install_path,
                             expected_bios,
                             &install_config),
            expected_success);

  if (!expected_success)
    return;

  EXPECT_EQ(install_config.slot, expected_slot);
  EXPECT_EQ(install_config.root.device(), expected_root);
  EXPECT_EQ(install_config.kernel.device(), expected_kernel);
  EXPECT_EQ(install_config.boot.device(), expected_boot);
  EXPECT_EQ(install_config.bios_type, expected_bios);
}

void TestStrToBiosType(string name,
                       bool expected_success,
                       BiosType expected_result) {
  BiosType bios_type = kBiosTypeUnknown;

  EXPECT_EQ(StrToBiosType(name, &bios_type),
            expected_success);

  if (!expected_success)
    return;

  EXPECT_EQ(bios_type, expected_result);
}

void TestKernelConfigToBiosType(string kernel_config,
                                bool expected_success,
                                BiosType expected_result) {
  BiosType bios_type = kBiosTypeUnknown;

  EXPECT_EQ(KernelConfigToBiosType(kernel_config, &bios_type),
            expected_success);

  if (!expected_success)
    return;

  EXPECT_EQ(bios_type, expected_result);
}

class InstallConfigTest : public ::testing::Test { };

TEST(InstallConfigTest, ConfigureInstallTest) {
  TestConfigureInstall("/dev/sda3", "/mnt",
                       true,
                       "A", "/dev/sda3", "/dev/sda2", "/dev/sda12");
  TestConfigureInstall("/dev/sda5", "/mnt",
                       true,
                       "B", "/dev/sda5", "/dev/sda4", "/dev/sda12");
  TestConfigureInstall("/dev/mmcblk0p3", "/mnt",
                       true, "A",
                       "/dev/mmcblk0p3", "/dev/mmcblk0p2", "/dev/mmcblk0p12");
  TestConfigureInstall("/dev/mmcblk0p5", "/mnt",
                       true, "B",
                       "/dev/mmcblk0p5", "/dev/mmcblk0p4", "/dev/mmcblk0p12");
  TestConfigureInstall("/dev/sda2", "/mnt",
                       false, "", "", "", "");
  TestConfigureInstall("/dev/sda", "/mnt",
                       false, "", "", "", "");
}

TEST(InstallConfigTest, StrToBiosTypeTest) {
  TestStrToBiosType("secure", true, kBiosTypeSecure);
  TestStrToBiosType("uboot", true, kBiosTypeUBoot);
  TestStrToBiosType("legacy", true, kBiosTypeLegacy);
  TestStrToBiosType("efi", true, kBiosTypeEFI);
  TestStrToBiosType("fuzzy", false, kBiosTypeUnknown);
}

TEST(InstallConfigTest, KernelConfigToBiosTypeTest) {
  BiosType legacy_bios = kBiosTypeLegacy;
#ifdef __arm__
  legacy_bios = kBiosTypeUBoot;
#endif

  TestKernelConfigToBiosType("kernel_config cros_secure",
                             true, kBiosTypeSecure);
  TestKernelConfigToBiosType("cros_legacy kernel_config",
                             true, legacy_bios);
  TestKernelConfigToBiosType("kernel_config cros_efi foo",
                             true, kBiosTypeEFI);
  TestKernelConfigToBiosType("kernel_config",
                             false, kBiosTypeUnknown);
}

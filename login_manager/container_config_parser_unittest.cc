// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/container_config_parser.h"

#include <stddef.h>
#include <sys/mount.h>

#include <base/values.h>
#include <gtest/gtest.h>

#include <libcontainer/libcontainer.h>

namespace {

const char kBasicJsonConfigData[] = R"json(
    {
      "process": {
        "user": {
          "uid": 100,
          "gid": 200
        },
        "args": [
          "/sbin/init"
        ]
      },
      "root": {
        "path": "rootfs_path"
      },
      "mounts": [
        {
          "name": "mount_0",
          "path": "/mount_0_path"
        },
        {
          "name": "mount_1",
          "path": "/mount_1_path"
        }
      ]
    }
)json";

const char kBasicJsonRuntimeData[] = R"json(
    {
      "mounts": {
        "mount_0": {
          "source": "tmpfs_source",
          "type": "tmpfs",
          "options": [ "nodev" ]
        },
        "mount_1": {
          "source": "/bind_source",
          "type": "bind",
          "options": [ "bind", "mount_outside", "noexec" ]
        }
      },
      "linux": {
        "uidMappings": "0 0 0",
        "gidMappings": "0 0 0",
        "devices": [
          {
            "path": "/dev/null",
            "type": 99,
            "major": 1,
            "minor": 3,
            "permissions": "rw",
            "fileMode": 438,
            "uid": 100,
            "gid": 200
          }
        ],
        "altSysCallTable": "android"
    }
  }
)json";

const std::string kExtraMountJsonConfigData = R"json(
    {
      "process": {
        "user": {
          "uid": 100,
          "gid": 200
        },
        "args": [
          "/sbin/init"
        ]
      },
      "root": {
        "path": "rootfs_path"
      },
      "mounts": [
        {
          "name": "mount_0",
          "path": "/mount_0_path"
        },
        {
          "name": "unknown",
          "path": "/unknown_path"
        },
        {
          "name": "mount_1",
          "path": "/mount_1_path"
        }
      ]
    })json";

}  // anonymous namespace

namespace login_manager {

TEST(ContainerConfigParserTest, TestBasicConfig) {
  const base::FilePath kNamedContainerPath("/var/run/containers/testc");

  ContainerConfigPtr config(container_config_create(),
                            &container_config_destroy);
  EXPECT_TRUE(ParseContainerConfig(kBasicJsonConfigData,
                                   kBasicJsonRuntimeData, "testc",
                                   kNamedContainerPath, &config));
  EXPECT_EQ(kNamedContainerPath.Append("rootfs_path").value(),
            container_config_get_rootfs(config.get()));
  EXPECT_EQ(1, container_config_get_num_program_args(config.get()));
  EXPECT_EQ(std::string("/sbin/init"),
            container_config_get_program_arg(config.get(), 0));
  EXPECT_EQ(100, container_config_get_uid(config.get()));
  EXPECT_EQ(200, container_config_get_gid(config.get()));
}

TEST(ContainerConfigParserTest, TestBasicConfigAndroid) {
  const base::FilePath kNamedContainerPath("/var/run/containers/android");
  const base::FilePath kSetfilesPath("/sbin/setfiles");

  ContainerConfigPtr config(container_config_create(),
                            &container_config_destroy);
  EXPECT_TRUE(ParseContainerConfig(kBasicJsonConfigData,
                                   kBasicJsonRuntimeData, "android",
                                   kNamedContainerPath, &config));
  EXPECT_EQ(kNamedContainerPath.Append("rootfs_path").value(),
            container_config_get_rootfs(config.get()));
  EXPECT_EQ(1, container_config_get_num_program_args(config.get()));
  EXPECT_EQ(std::string("/sbin/init"),
            container_config_get_program_arg(config.get(), 0));
  EXPECT_EQ(std::string(kSetfilesPath.value()),
            container_config_get_run_setfiles(config.get()));
}

TEST(ContainerConfigParserTest, TestFailedConfigRootDictEmpty) {
  const std::string kEmptyJsonConfigData = "{}";
  const base::FilePath kNamedContainerPath("/var/run/containers/testc");

  ContainerConfigPtr config(container_config_create(),
                            &container_config_destroy);
  EXPECT_FALSE(ParseContainerConfig(kEmptyJsonConfigData,
                                    kBasicJsonRuntimeData, "testc",
                                    kNamedContainerPath, &config));
}

TEST(ContainerConfigParserTest, TestFailedConfigUnknownMount) {
  // A mount specified in config but not detailed in runtime should fail.
  const base::FilePath kNamedContainerPath("/var/run/containers/testc");

  ContainerConfigPtr config(container_config_create(),
                            &container_config_destroy);
  EXPECT_FALSE(ParseContainerConfig(kExtraMountJsonConfigData,
                                    kBasicJsonRuntimeData, "testc",
                                    kNamedContainerPath, &config));
}

}  // namespace login_manager

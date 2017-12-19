// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <iostream>
#include <memory>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "imageloader/helper_process.h"
#include "imageloader/imageloader.h"
#include "imageloader/imageloader_impl.h"
#include "imageloader/mount_helper.h"

namespace {

constexpr uint8_t kProdPublicKey[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x53, 0xd9, 0x6f, 0xb1, 0x92, 0x97, 0x39, 0xa9, 0x97,
    0x18, 0xbe, 0xa7, 0x97, 0x15, 0x06, 0x27, 0x9c, 0x55, 0xa5, 0x40, 0xc1,
    0x0f, 0x98, 0xfa, 0xd8, 0x61, 0x18, 0xee, 0xcf, 0xf3, 0xbb, 0xf9, 0x6e,
    0x6d, 0xa0, 0x66, 0xd2, 0x29, 0xf0, 0x78, 0x5b, 0x7a, 0xab, 0x54, 0xca,
    0x53, 0x16, 0xb0, 0xf9, 0xc4, 0xd8, 0x1d, 0x93, 0x5b, 0x83, 0x6e, 0xa5,
    0x65, 0xe5, 0x71, 0xbc, 0x8d, 0x72, 0x02};

// The path where the components are stored on the device.
constexpr char kComponentsPath[] = "/var/lib/imageloader";
// The location of the container public key.
constexpr char kContainerPublicKeyPath[] =
    "/usr/share/misc/oci-container-key-pub.der";

bool LoadKeyFromFile(const std::string& file, std::vector<uint8_t>* key_out) {
  CHECK(key_out);

  base::FilePath key_file(file);
  std::string key_data;

  // The key should be pretty small.
  if (!ReadFileToString(key_file, &key_data)) {
    LOG(WARNING) << "Could not read key file " << key_file.value();
    return false;
  }

  key_out->clear();
  key_out->insert(key_out->begin(), key_data.begin(), key_data.end());

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_bool(dry_run, false,
              "Changes unmount_all to print the paths which would be "
              "affected.");
  DEFINE_bool(mount, false,
              "Rather than starting a dbus daemon, verify and mount a single "
              "component and exit immediately.");
  DEFINE_string(mount_component, "",
                "Specifies the name of the component when using --mount.");
  DEFINE_string(mount_point, "",
                "Specifies the mountpoint when using either --mount or "
                "--unmount.");
  DEFINE_string(loaded_mounts_base, imageloader::ImageLoader::kLoadedMountsBase,
                "Base path where components are mounted (unless --mount_point "
                "is used).");
  DEFINE_int32(mount_helper_fd, -1,
               "Control socket for starting an ImageLoader subprocess. Used "
               "internally.");
  DEFINE_bool(unmount, false,
              "Unmounts the path specified by mount_point and exits "
              "immediately.");
  DEFINE_bool(unmount_all, false,
              "Unmounts all the mountpoints under loaded_mounts_base and exits "
              "immediately.");
  brillo::FlagHelper::Init(argc, argv, "imageloader");

  brillo::OpenLog("imageloader", true);
  brillo::InitLog(brillo::kLogToSyslog);

  if (FLAGS_mount + FLAGS_unmount + FLAGS_unmount_all > 1) {
    LOG(ERROR) << "Only one of --mount, --unmount, and --unmount_all can be "
                  "set at a time.";
    return 1;
  }

  // Executing this as the helper process if specified.
  if (FLAGS_mount_helper_fd >= 0) {
    CHECK_GT(FLAGS_mount_helper_fd, -1);
    base::ScopedFD fd(FLAGS_mount_helper_fd);
    imageloader::MountHelper mount_helper(std::move(fd));
    return mount_helper.Run();
  }

  imageloader::Keys keys;
  // The order of key addition below is important.
  // 1. Prod key, used to sign Flash.
  keys.push_back(std::vector<uint8_t>(std::begin(kProdPublicKey),
                                      std::end(kProdPublicKey)));
  // 2. Container key.
  std::vector<uint8_t> container_key;
  if (LoadKeyFromFile(kContainerPublicKeyPath, &container_key))
    keys.push_back(container_key);

  imageloader::ImageLoaderConfig config(keys, kComponentsPath,
                                        FLAGS_loaded_mounts_base.c_str());
  auto helper_process = std::make_unique<imageloader::HelperProcess>();
  helper_process->Start(argc, argv, "--mount_helper_fd");

  // Load and mount the specified component and exit.
  if (FLAGS_mount) {
    // Run with minimal privilege.
    imageloader::ImageLoader::EnterSandbox();

    if (FLAGS_mount_point.empty() || FLAGS_mount_component.empty()) {
      LOG(ERROR) << "--mount_component=name and --mount_point=path must be set "
                    "with --mount";
      return 1;
    }
    // Access the ImageLoaderImpl directly to avoid needless dbus dependencies,
    // which may not be available at early boot.
    imageloader::ImageLoaderImpl loader(std::move(config));

    std::string flash_version =
        loader.GetComponentVersion(FLAGS_mount_component);
    // imageloader returns "" if the component doesn't exist. In this case
    // return 0 so our crash reporting doesn't think something actually went
    // wrong.
    if (flash_version.empty())
      return 0;

    if (!loader.LoadComponent(FLAGS_mount_component, FLAGS_mount_point,
                              helper_process.get())) {
      LOG(ERROR) << "Failed to verify and mount component: "
                 << FLAGS_mount_component << " at " << FLAGS_mount_point;
      return 1;
    }
    return 0;
  }

  // Unmount all component mount points and exit.
  if (FLAGS_unmount_all) {
    // Run with minimal privilege.
    imageloader::ImageLoader::EnterSandbox();

    imageloader::ImageLoaderImpl loader(std::move(config));
    std::vector<std::string> paths;
    const base::FilePath parent_dir(FLAGS_loaded_mounts_base);
    bool success = loader.CleanupAll(FLAGS_dry_run,
                                     parent_dir, &paths, helper_process.get());
    if (FLAGS_dry_run) {
      for (const auto& path : paths) {
        std::cout << path << "\n";
      }
    }
    if (!success) {
      LOG(ERROR) << "--unmount_all failed!";
      return 1;
    }
    return 0;
  }

  // Unmount a component mount point and exit.
  if (FLAGS_unmount) {
    // Run with minimal privilege.
    imageloader::ImageLoader::EnterSandbox();

    if (FLAGS_mount_point.empty()) {
      LOG(ERROR) << "--mount_point=path must be set with --unmount";
      return 1;
    }

    imageloader::ImageLoaderImpl loader(std::move(config));
    const base::FilePath path(FLAGS_mount_point);
    bool success = loader.Cleanup(path, helper_process.get());
    if (!success) {
      LOG(ERROR) << "--unmount failed!";
      return 1;
    }
    return 0;
  }

  // Run as a daemon and wait for dbus requests.
  imageloader::ImageLoader daemon(std::move(config), std::move(helper_process));
  daemon.Run();

  return 0;
}

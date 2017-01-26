// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <base/memory/ptr_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>

#include "imageloader.h"
#include "imageloader_impl.h"
#include "verity_mounter.h"

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
// The base path where the components are mounted.
constexpr char kMountPath[] = "/run/imageloader";

int main(int argc, char** argv) {
  DEFINE_bool(mount, false,
              "Rather than starting a dbus daemon, verify and mount a single "
              "component and exit immediately.");
  DEFINE_string(mount_component, "",
                "Specifies the name of the component when using --mount.");
  DEFINE_string(mount_point, "",
                "Specifies the mountpoint when using --mount.");
  brillo::FlagHelper::Init(argc, argv, "imageloader");

  brillo::OpenLog("imageloader", true);
  brillo::InitLog(brillo::kLogToSyslog);

  std::vector<uint8_t> key(std::begin(kProdPublicKey),
                           std::end(kProdPublicKey));
  auto verity_mounter = base::MakeUnique<imageloader::VerityMounter>();
  imageloader::ImageLoaderConfig config(key, kComponentsPath, kMountPath,
                                        std::move(verity_mounter));

  if (FLAGS_mount) {
    if (FLAGS_mount_point == "" || FLAGS_mount_component == "") {
      LOG(FATAL) << "--mount_component=name and --mount_point=path must be set "
                    "with --mount";
    }
    // Access the ImageLoaderImpl directly to avoid needless dbus dependencies,
    // which may not be available at early boot.
    imageloader::ImageLoaderImpl loader(std::move(config));
    if (!loader.LoadComponent(FLAGS_mount_component, FLAGS_mount_point)) {
      LOG(FATAL) << "Failed to verify and mount component.";
    }
    return 0;
  }

  imageloader::ImageLoader daemon(std::move(config));
  daemon.Run();

  return 0;
}

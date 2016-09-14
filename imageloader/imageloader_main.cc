// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <brillo/flag_helper.h>

#include "imageloader.h"
#include "imageloader_common.h"
#include "imageloader_impl.h"
#include "loop_mounter.h"

// TODO(kerrnel): Switch to the prod keys before shipping this feature.
const uint8_t kDevPublicKey[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x7a, 0xaa, 0x2b, 0xf9, 0x3d, 0x7a, 0xbe, 0x35, 0x9a,
    0xfc, 0x9f, 0x39, 0x2d, 0x2d, 0x37, 0x07, 0xd4, 0x19, 0x67, 0x67, 0x30,
    0xbb, 0x5c, 0x74, 0x22, 0xd5, 0x02, 0x07, 0xaf, 0x6b, 0x12, 0x9d, 0x12,
    0xf0, 0x34, 0xfd, 0x1a, 0x7f, 0x02, 0xd8, 0x46, 0x2b, 0x25, 0xca, 0xa0,
    0x6e, 0x2b, 0x54, 0x41, 0xee, 0x92, 0xa2, 0x0f, 0xa2, 0x2a, 0xc0, 0x30,
    0xa6, 0x8c, 0xd1, 0x16, 0x0a, 0x48, 0xca};

// The path where the components are stored on the device.
const char kComponentsPath[] = "/mnt/stateful_partition/encrypted/imageloader";
// The base path where the components are mounted.
const char kMountPath[] = "/mnt/stateful_partition/imageloader_mounts";

int main(int argc, char** argv) {
  signal(SIGTERM, imageloader::OnQuit);
  signal(SIGINT, imageloader::OnQuit);

  DEFINE_bool(o, false, "run once");
  DEFINE_bool(mount, false,
              "Rather than starting a dbus daemon, verify and mount a single "
              "component and exit immediately.");
  DEFINE_string(mount_component, "",
                "Specifies the name of the component when using --mount.");
  DEFINE_string(mount_point, "",
                "Specifies the mountpoint when using --mount.");
  brillo::FlagHelper::Init(argc, argv, "imageloader");

  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  std::vector<uint8_t> key(std::begin(kDevPublicKey), std::end(kDevPublicKey));
  auto loop_mounter = base::MakeUnique<imageloader::LoopMounter>();
  imageloader::ImageLoaderConfig config(key, kComponentsPath, kMountPath,
                                        std::move(loop_mounter));

  if (FLAGS_mount) {
    if (FLAGS_mount_point == "" || FLAGS_mount_component == "") {
      LOG(ERROR) << "--mount_component=name and --mount_point=path must be set "
                    "with --mount";
      return 1;
    }
    // Access the ImageLoaderImpl directly to avoid needless dbus dependencies,
    // which may not be available at early boot.
    imageloader::ImageLoaderImpl loader(std::move(config));
    return loader.LoadComponent(FLAGS_mount_component, FLAGS_mount_point) ? 0
                                                                          : 1;
  }

  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(imageloader::kImageLoaderName);
  imageloader::ImageLoader helper(&conn, std::move(config));

  if (FLAGS_o) {
    dispatcher.dispatch_pending();
  } else {
    dispatcher.enter();
  }

  return 0;
}

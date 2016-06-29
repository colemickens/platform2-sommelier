// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <brillo/flag_helper.h>

#include "imageloader.h"
#include "imageloader_common.h"

int main(int argc, char** argv) {
  signal(SIGTERM, imageloader::OnQuit);
  signal(SIGINT, imageloader::OnQuit);

  DEFINE_bool(o, false, "run once");
  brillo::FlagHelper::Init(argc, argv, "imageloader");

  logging::LoggingSettings settings;
  logging::InitLogging(settings);

  DBus::BusDispatcher dispatcher;
  DBus::default_dispatcher = &dispatcher;
  DBus::Connection conn = DBus::Connection::SystemBus();
  conn.request_name(imageloader::kImageLoaderName);
  imageloader::ImageLoader helper(&conn);

  if (FLAGS_o) {
    dispatcher.dispatch_pending();
  } else {
    dispatcher.enter();
  }
  return 0;
}

// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "image_burner.h"
#include "image_burner_impl.h"
#include "image_burner_utils.h"
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/logging.h>

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = FILE_PATH_LITERAL("/var/log/image_burner.log");
  settings.lock_log = logging::LOCK_LOG_FILE;
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  settings.dcheck_state =
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS;
  logging::InitLogging(settings);

  g_type_init();

  imageburn::BurnWriter writer;
  imageburn::BurnReader reader;
  imageburn::BurnRootPathGetter path_getter;
  scoped_ptr<imageburn::BurnerImpl> burner(
      new imageburn::BurnerImpl(&writer, &reader, NULL, &path_getter));

  imageburn::ImageBurnService service(burner.get());
  service.Initialize();
  burner->InitSignalSender(&service);

  service.Register(chromeos::dbus::GetSystemBusConnection());
  service.Run();
  return 0;
}

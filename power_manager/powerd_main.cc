// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdk.h>
#include <gflags/gflags.h>

#include <string>

#include "base/logging.h"
#include "cros/chromeos_cros_api.h"
#include "power_manager/powerd.h"

using std::string;

DEFINE_string(prefs_dir, "",
              "Directory to store logs and settings.");

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(!FLAGS_prefs_dir.empty()) << "--prefs_dir is required";
  CHECK(argc == 1) << "Unexpected arguments. Try --help";
  FilePath prefs_dir(FLAGS_prefs_dir);
  power_manager::PowerPrefs prefs(prefs_dir);
  string err;
  CHECK(chromeos::LoadLibcros(chromeos::kCrosDefaultPath, err))
      << "LoadLibcros('" << chromeos::kCrosDefaultPath << "') failed: " << err;
  gdk_init(&argc, &argv);
  power_manager::Backlight backlight;
  CHECK(backlight.Init()) << "Cannot initialize backlight";
  power_manager::BacklightController backlight_ctl(&backlight, &prefs);
  CHECK(backlight_ctl.Init()) << "Cannot initialize backlight controller";
  power_manager::Daemon daemon(&backlight_ctl, &prefs);
  daemon.Init();
  daemon.Run();
  return 0;
}

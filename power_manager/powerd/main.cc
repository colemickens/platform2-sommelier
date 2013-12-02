// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <glib.h>

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "metrics/metrics_library.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/daemon.h"
#include "power_manager/powerd/policy/external_backlight_controller.h"
#include "power_manager/powerd/policy/internal_backlight_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"
#include "power_manager/powerd/system/display_power_setter.h"
#include "power_manager/powerd/system/internal_backlight.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

using power_manager::Daemon;
using power_manager::Prefs;
using power_manager::policy::BacklightController;
using power_manager::policy::ExternalBacklightController;
using power_manager::policy::InternalBacklightController;
using power_manager::policy::KeyboardBacklightController;
using power_manager::system::AmbientLightSensor;
using power_manager::system::DisplayPowerSetter;
using power_manager::system::InternalBacklight;
using std::string;

DEFINE_string(prefs_dir, power_manager::kReadWritePrefsDir,
              "Directory holding read/write preferences.");
DEFINE_string(default_prefs_dir, power_manager::kReadOnlyPrefsDir,
              "Directory holding read-only default settings.");
DEFINE_string(log_dir, "", "Directory where logs are written.");
DEFINE_string(run_dir, "", "Directory where stateful data is written.");
// This flag is handled by libbase/libchrome's logging library instead of
// directly by powerd, but it is defined here so gflags won't abort after
// seeing an unexpected flag.
DEFINE_string(vmodule, "",
              "Per-module verbose logging levels, e.g. \"foo=1,bar=2\"");

namespace {

// Moves |latest_log_symlink| to |previous_log_symlink| and creates a relative
// symlink at |latest_log_symlink| pointing to |log_file|. All files must be in
// the same directory.
void UpdateLogSymlinks(const base::FilePath& latest_log_symlink,
                       const base::FilePath& previous_log_symlink,
                       const base::FilePath& log_file) {
  CHECK_EQ(latest_log_symlink.DirName().value(), log_file.DirName().value());
  file_util::Delete(previous_log_symlink, false);
  file_util::Move(latest_log_symlink, previous_log_symlink);
  if (!file_util::CreateSymbolicLink(log_file.BaseName(), latest_log_symlink)) {
    LOG(ERROR) << "Unable to create symbolic link from "
               << latest_log_symlink.value() << " to " << log_file.value();
  }
}

string GetTimeAsString(time_t utime) {
  struct tm tm;
  CHECK_EQ(localtime_r(&utime, &tm), &tm);
  char str[16];
  CHECK_EQ(strftime(str, sizeof(str), "%Y%m%d-%H%M%S", &tm), 15UL);
  return string(str);
}

// Convenience method that returns true if |name| exists and is true.
bool BoolPrefIsTrue(Prefs* prefs, const std::string& name) {
  bool value = false;
  return prefs->GetBool(name, &value) && value;
}

}  // namespace

int main(int argc, char* argv[]) {
  // gflags rewrites argv; give base::CommandLine first crack at it.
  CommandLine::Init(argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(!FLAGS_prefs_dir.empty()) << "--prefs_dir is required";
  CHECK(!FLAGS_log_dir.empty()) << "--log_dir is required";
  CHECK(!FLAGS_run_dir.empty()) << "--run_dir is required";
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";

  const base::FilePath log_file = base::FilePath(FLAGS_log_dir).Append(
      StringPrintf("powerd.%s", GetTimeAsString(::time(NULL)).c_str()));
  UpdateLogSymlinks(base::FilePath(FLAGS_log_dir).Append("powerd.LATEST"),
                    base::FilePath(FLAGS_log_dir).Append("powerd.PREVIOUS"),
                    log_file);
  logging::InitLogging(log_file.value().c_str(),
                       logging::LOG_ONLY_TO_FILE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  LOG(INFO) << "vcsid " << VCSID;

  Prefs prefs;
  CHECK(prefs.Init(power_manager::util::GetPrefPaths(FLAGS_prefs_dir,
                                                     FLAGS_default_prefs_dir)));
  g_type_init();

  scoped_ptr<AmbientLightSensor> light_sensor;
  if (BoolPrefIsTrue(&prefs, power_manager::kHasAmbientLightSensorPref)) {
    light_sensor.reset(new power_manager::system::AmbientLightSensor());
    light_sensor->Init();
  }

  DisplayPowerSetter display_power_setter;
  scoped_ptr<InternalBacklight> display_backlight;
  scoped_ptr<BacklightController> display_backlight_controller;
  if (BoolPrefIsTrue(&prefs, power_manager::kExternalDisplayOnlyPref)) {
    display_backlight_controller.reset(
        new ExternalBacklightController(&display_power_setter));
    static_cast<ExternalBacklightController*>(
        display_backlight_controller.get())->Init();
  } else {
    display_backlight.reset(new InternalBacklight);
    if (!display_backlight->Init(
            base::FilePath(power_manager::kInternalBacklightPath),
            power_manager::kInternalBacklightPattern)) {
      LOG(ERROR) << "Cannot initialize display backlight";
      display_backlight.reset();
    } else {
      display_backlight_controller.reset(
          new InternalBacklightController(
              display_backlight.get(), &prefs, light_sensor.get(),
              &display_power_setter));
      if (!static_cast<InternalBacklightController*>(
              display_backlight_controller.get())->Init()) {
        LOG(ERROR) << "Cannot initialize display backlight controller";
        display_backlight_controller.reset();
        display_backlight.reset();
      }
    }
  }

  scoped_ptr<InternalBacklight> keyboard_backlight;
  scoped_ptr<KeyboardBacklightController> keyboard_backlight_controller;
  if (BoolPrefIsTrue(&prefs, power_manager::kHasKeyboardBacklightPref)) {
    if (!light_sensor.get()) {
      LOG(ERROR) << "Keyboard backlight requires ambient light sensor";
    } else {
      keyboard_backlight.reset(new InternalBacklight);
      if (!keyboard_backlight->Init(
              base::FilePath(power_manager::kKeyboardBacklightPath),
              power_manager::kKeyboardBacklightPattern)) {
        LOG(ERROR) << "Cannot initialize keyboard backlight";
        keyboard_backlight.reset();
      } else {
        keyboard_backlight_controller.reset(
            new KeyboardBacklightController(
                keyboard_backlight.get(), &prefs, light_sensor.get(),
                display_backlight_controller.get()));
        if (!keyboard_backlight_controller->Init()) {
          LOG(ERROR) << "Cannot initialize keyboard backlight controller";
          keyboard_backlight_controller.reset();
          keyboard_backlight.reset();
        }
      }
    }
  }

  MetricsLibrary metrics_lib;
  metrics_lib.Init();
  base::FilePath run_dir(FLAGS_run_dir);
  Daemon daemon(&prefs,
                &metrics_lib,
                display_backlight_controller.get(),
                keyboard_backlight_controller.get(),
                run_dir);
  daemon.Init();
  daemon.Run();
  return 0;
}

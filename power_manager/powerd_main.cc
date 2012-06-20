// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <glib.h>
#include <syslog.h>

#include <string>

// Defines from syslog.h that conflict with base/logging.h. Ugh.
#undef LOG_INFO
#undef LOG_WARNING

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "power_manager/ambient_light_sensor.h"
#include "power_manager/audio_detector.h"
#include "power_manager/backlight.h"
#include "power_manager/idle_detector.h"
#include "power_manager/monitor_reconfigure.h"
#include "power_manager/power_constants.h"
#include "power_manager/powerd.h"
#include "power_manager/video_detector.h"

#ifdef IS_DESKTOP
#include "power_manager/external_backlight_client.h"
#include "power_manager/external_backlight_controller.h"
#else
#include "power_manager/internal_backlight_controller.h"
#endif

#ifndef VCSID
#define VCSID "<not set>"
#endif

using std::string;

DEFINE_string(prefs_dir, "",
              "Directory to store settings.");
DEFINE_string(default_prefs_dir, "",
              "Directory to read default settings (Read Only).");
DEFINE_string(log_dir, "",
              "Directory to store logs.");
DEFINE_string(run_dir, "",
              "Directory to store stateful data for daemon.");

// Returns true on success.
static bool SetUpLogSymlink(const string& symlink_path,
                            const string& log_basename) {
  unlink(symlink_path.c_str());
  if (symlink(log_basename.c_str(), symlink_path.c_str()) == -1) {
    PLOG(ERROR) << "Unable to create symlink " << symlink_path
                << " pointing at " << log_basename;
    return false;
  }
  return true;
}

static string GetTimeAsString(time_t utime) {
  struct tm tm;
  CHECK_EQ(localtime_r(&utime, &tm), &tm);
  char str[16];
  CHECK_EQ(strftime(str, sizeof(str), "%Y%m%d-%H%M%S", &tm), 15UL);
  return string(str);
}

int main(int argc, char* argv[]) {
  // Sadly we can't use LOG() here - we always want this message logged, even
  // when other logging is turned off.
  openlog("powerd", LOG_PID, LOG_DAEMON);
  syslog(LOG_NOTICE, "vcsid %s", VCSID);
  closelog();
  google::ParseCommandLineFlags(&argc, &argv, true);
  CHECK(!FLAGS_prefs_dir.empty()) << "--prefs_dir is required";
  CHECK(!FLAGS_log_dir.empty()) << "--log_dir is required";
  CHECK(!FLAGS_run_dir.empty()) << "--run_dir is required";
  CHECK_EQ(argc, 1) << "Unexpected arguments. Try --help";
  CommandLine::Init(argc, argv);

  const string log_latest =
      StringPrintf("%s/powerd.LATEST", FLAGS_log_dir.c_str());
  const string log_basename =
      StringPrintf("powerd.%s", GetTimeAsString(::time(NULL)).c_str());
  const string log_path = FLAGS_log_dir + "/" + log_basename;
  CHECK(SetUpLogSymlink(log_latest, log_basename));
  logging::InitLogging(log_path.c_str(),
                       logging::LOG_ONLY_TO_FILE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE,
                       logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  FilePath prefs_dir(FLAGS_prefs_dir);
  FilePath default_prefs_dir(FLAGS_default_prefs_dir.empty() ?
                             "/usr/share/power_manager" :
                             FLAGS_default_prefs_dir);
  power_manager::PowerPrefs prefs(prefs_dir, default_prefs_dir);
  g_type_init();

  power_manager::MonitorReconfigure monitor_reconfigure;
#ifdef IS_DESKTOP
  power_manager::ExternalBacklightClient backlight;
  if (!backlight.Init())
    LOG(WARNING) << "Cannot initialize backlight";
#else
  power_manager::Backlight backlight;
  if (!backlight.Init(FilePath(power_manager::kBacklightPath),
                      power_manager::kBacklightPattern))
    LOG(WARNING) << "Cannot initialize backlight";
#endif

#ifdef IS_DESKTOP
  power_manager::ExternalBacklightController backlight_ctl(&backlight);
#else
  power_manager::InternalBacklightController backlight_ctl(&backlight, &prefs);
#endif
  backlight_ctl.SetMonitorReconfigure(&monitor_reconfigure);
  if (!backlight_ctl.Init())
    LOG(WARNING) << "Cannot initialize backlight controller";

  power_manager::AmbientLightSensor als(&backlight_ctl, &prefs);
  if (!als.Init())
    LOG(WARNING) << "Cannot initialize light sensor";
  scoped_ptr<power_manager::Backlight> keylight;
#ifdef HAS_KEYBOARD_BACKLIGHT
  keylight.reset(new power_manager::Backlight());
  if (!keylight->Init(FilePath(power_manager::kKeyboardBacklightPath),
                      power_manager::kKeyboardBacklightPattern)) {
    keylight.reset();
    LOG(WARNING) << "Cannot initialize keyboard backlight";
  }
#endif
  MetricsLibrary metrics_lib;
  power_manager::VideoDetector video_detector;
  video_detector.Init();
  power_manager::AudioDetector audio_detector;
  audio_detector.Init();
  power_manager::IdleDetector idle;
  metrics_lib.Init();
  FilePath run_dir(FLAGS_run_dir);
  power_manager::Daemon daemon(&backlight_ctl,
                               &prefs,
                               &metrics_lib,
                               &video_detector,
                               &audio_detector,
                               &idle,
                               keylight.get(),
                               run_dir);

  daemon.Init();
  daemon.Run();
  return 0;
}

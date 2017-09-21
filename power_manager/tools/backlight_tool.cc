// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>

#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/internal_backlight_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/system/backlight_stub.h"
#include "power_manager/powerd/system/display/display_power_setter_stub.h"
#include "power_manager/powerd/system/internal_backlight.h"

using power_manager::Prefs;
using power_manager::TabletMode;
using power_manager::policy::BacklightController;
using power_manager::policy::InternalBacklightController;
using power_manager::policy::KeyboardBacklightController;
using power_manager::system::BacklightStub;
using power_manager::system::DisplayPowerSetterStub;
using power_manager::system::InternalBacklight;
using power_manager::util::ClampPercent;

namespace {

// Prints |message| to stderr with a trailing newline and exits.
void Abort(const std::string& message) {
  fprintf(stderr, "%s\n", message.c_str());
  exit(1);
}

// Converter instantiates several internal powerd classes to perform conversions
// between hardware backlight levels and the nonlinear percents that powerd
// uses (which are dependent on the powerd prefs that have been set for the
// device). It also supports linear percents.
class Converter {
 public:
  Converter(int64_t current_level, int64_t max_level, bool keyboard)
      : backlight_(max_level, current_level) {
    CHECK(prefs_.Init(Prefs::GetDefaultPaths()));
    if (keyboard) {
      auto controller = base::MakeUnique<KeyboardBacklightController>();
      controller->Init(&backlight_,
                       &prefs_,
                       nullptr /* sensor */,
                       nullptr /* display_backlight_controller */,
                       TabletMode::UNSUPPORTED);
      controller_ = std::move(controller);
    } else {
      auto controller = base::MakeUnique<InternalBacklightController>();
      controller->Init(
          &backlight_, &prefs_, nullptr /* sensor */, &display_power_setter_);
      controller_ = std::move(controller);
    }
  }

  // Converts a brightness level to a nonlinear percent in [0.0, 100.0].
  double LevelToNonlinearPercent(int64_t level) {
    return controller_->LevelToPercent(level);
  }

  // Converts a nonlinear percent in [0.0, 100.0] to a brightness level.
  int64_t NonlinearPercentToLevel(double percent) {
    return controller_->PercentToLevel(percent);
  }

  // Converts a brightness level to a linear percent in [0.0, 100.0].
  double LevelToLinearPercent(int64_t level) {
    return level * 100.0 / backlight_.GetMaxBrightnessLevel();
  }

  // Converts a linear percent in [0.0, 100.0] to a brightness level.
  int64_t LinearPercentToLevel(double percent) {
    return static_cast<int64_t>(roundl(
        ClampPercent(percent) * backlight_.GetMaxBrightnessLevel() / 100.0));
  }

 private:
  // A stub is used so |controller_| won't change the actual brightness.
  BacklightStub backlight_;
  Prefs prefs_;
  DisplayPowerSetterStub display_power_setter_;
  std::unique_ptr<BacklightController> controller_;

  DISALLOW_COPY_AND_ASSIGN(Converter);
};

}  // namespace

int main(int argc, char* argv[]) {
  // Flags that print the brightness.
  DEFINE_bool(get_brightness, false, "Print current brightness level");
  DEFINE_bool(get_brightness_percent,
              false,
              "Print current brightness as linear percent");
  DEFINE_bool(get_max_brightness, false, "Print max brightness level");

  // Flags that convert between units.
  DEFINE_double(nonlinear_to_level,
                -1.0,
                "Convert supplied nonlinear brightness percent to level");
  DEFINE_int64(level_to_nonlinear,
               -1,
               "Convert supplied brightness level to nonlinear percent");
  DEFINE_double(linear_to_level,
                -1.0,
                "Convert supplied linear brightness percent to level");
  DEFINE_int64(level_to_linear,
               -1,
               "Convert supplied brightness level to linear percent");
  DEFINE_double(linear_to_nonlinear,
                -1.0,
                "Convert supplied linear brightness percent to nonlinear");
  DEFINE_double(nonlinear_to_linear,
                -1.0,
                "Convert supplied nonlinear brightness percent to linear");

  // Flags that set the brightness.
  DEFINE_int64(set_brightness, -1, "Set brightness level");
  DEFINE_double(set_brightness_percent,
                -1.0,
                "Set brightness as "
                "linearly-calculated percent in [0.0, 100.0]");
  DEFINE_int64(set_resume_brightness,
               -2,
               "Set brightness level on resume; -1 clears current level");
  DEFINE_double(set_resume_brightness_percent,
                -1.0,
                "Set resume brightness as linearly-calculated percent in "
                "[0.0, 100.0]");

  // Other flags.
  DEFINE_bool(keyboard, false, "Use keyboard (rather than panel) backlight");

  brillo::FlagHelper::Init(
      argc,
      argv,
      "Print or set the internal panel or keyboard backlight's brightness.");

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  logging::SetMinLogLevel(logging::LOG_WARNING);

  if (FLAGS_get_brightness + FLAGS_get_max_brightness +
          FLAGS_get_brightness_percent + (FLAGS_nonlinear_to_level >= 0.0) +
          (FLAGS_level_to_nonlinear >= 0) + (FLAGS_linear_to_level >= 0.0) +
          (FLAGS_level_to_linear >= 0) + (FLAGS_linear_to_nonlinear >= 0.0) +
          (FLAGS_nonlinear_to_linear >= 0.0) >
      1) {
    Abort("At most one flag that prints a level or percent may be passed.");
  }
  if (FLAGS_set_brightness >= 0 && FLAGS_set_brightness_percent >= 0.0)
    Abort("At most one of -set_brightness* may be passed.");
  if (FLAGS_set_resume_brightness >= 0 &&
      FLAGS_set_resume_brightness_percent >= 0.0) {
    Abort("At most one of -set_resume_brightness* may be passed.");
  }

  InternalBacklight backlight;
  CHECK(backlight.Init(
      base::FilePath(FLAGS_keyboard ? power_manager::kKeyboardBacklightPath
                                    : power_manager::kInternalBacklightPath),
      FLAGS_keyboard ? power_manager::kKeyboardBacklightPattern
                     : power_manager::kInternalBacklightPattern));

  const int64_t current_level = backlight.GetCurrentBrightnessLevel();
  Converter converter(
      current_level, backlight.GetMaxBrightnessLevel(), FLAGS_keyboard);

  // Print brightness.
  if (FLAGS_get_brightness)
    printf("%" PRIi64 "\n", current_level);
  if (FLAGS_get_max_brightness)
    printf("%" PRIi64 "\n", backlight.GetMaxBrightnessLevel());
  if (FLAGS_get_brightness_percent)
    printf("%f\n", converter.LevelToLinearPercent(current_level));

  // Convert between units.
  if (FLAGS_nonlinear_to_level >= 0.0) {
    printf("%" PRIi64 "\n",
           converter.NonlinearPercentToLevel(FLAGS_nonlinear_to_level));
  }
  if (FLAGS_level_to_nonlinear >= 0) {
    printf("%f\n", converter.LevelToNonlinearPercent(FLAGS_level_to_nonlinear));
  }
  if (FLAGS_linear_to_level >= 0.0) {
    printf("%" PRIi64 "\n",
           converter.LinearPercentToLevel(FLAGS_linear_to_level));
  }
  if (FLAGS_level_to_linear >= 0) {
    printf("%f\n", converter.LevelToLinearPercent(FLAGS_level_to_linear));
  }
  if (FLAGS_linear_to_nonlinear >= 0.0) {
    printf("%f\n",
           converter.LevelToNonlinearPercent(
               converter.LinearPercentToLevel(FLAGS_linear_to_nonlinear)));
  }
  if (FLAGS_nonlinear_to_linear >= 0.0) {
    printf("%f\n",
           converter.LevelToLinearPercent(
               converter.NonlinearPercentToLevel(FLAGS_nonlinear_to_linear)));
  }

  // Change the brightness.
  if (FLAGS_set_brightness >= 0) {
    CHECK(
        backlight.SetBrightnessLevel(FLAGS_set_brightness, base::TimeDelta()));
  }
  if (FLAGS_set_brightness_percent >= 0.0) {
    CHECK(backlight.SetBrightnessLevel(
        converter.LinearPercentToLevel(FLAGS_set_brightness_percent),
        base::TimeDelta()));
  }
  if (FLAGS_set_resume_brightness >= -1)  // -1 clears
    CHECK(backlight.SetResumeBrightnessLevel(FLAGS_set_resume_brightness));
  if (FLAGS_set_resume_brightness_percent >= 0.0) {
    CHECK(backlight.SetResumeBrightnessLevel(
        converter.LinearPercentToLevel(FLAGS_set_resume_brightness_percent)));
  }

  return 0;
}

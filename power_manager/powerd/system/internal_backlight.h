// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INTERNAL_BACKLIGHT_H_
#define POWER_MANAGER_POWERD_SYSTEM_INTERNAL_BACKLIGHT_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "power_manager/common/signal_callback.h"
#include "power_manager/powerd/system/backlight_interface.h"

typedef int gboolean;
typedef unsigned int guint;

namespace power_manager {
namespace system {

// Controls an LCD or keyboard backlight via sysfs.
class InternalBacklight : public BacklightInterface {
 public:
  // Base names of backlight files within sysfs directories.
  static const char kBrightnessFilename[];
  static const char kMaxBrightnessFilename[];
  static const char kActualBrightnessFilename[];

  InternalBacklight();
  virtual ~InternalBacklight();

  // Initialize the backlight object.
  //
  // The |base_path| specifies the directory to look for backlights.  The
  // |pattern| is a glob pattern to help find the right backlight.
  // Expected values for parameters look like:
  //   base: "/sys/class/backlight", pattern: "*"
  //   base: "/sys/class/leds", pattern: "*:kbd_backlight"
  //
  // On success, return true; otherwise return false.
  bool Init(const FilePath& base_path, const FilePath::StringType& pattern);

  bool transition_timeout_is_set() const { return transition_timeout_id_ != 0; }
  void set_current_time_for_testing(base::TimeTicks now) {
    current_time_for_testing_ = now;
  }

  // Calls HandleTransitionTimeout() as if |transition_timeout_id_| had fired
  // and returns its return value.
  gboolean TriggerTransitionTimeoutForTesting();

  // Overridden from BacklightInterface:
  virtual bool GetMaxBrightnessLevel(int64* max_level);
  virtual bool GetCurrentBrightnessLevel(int64* current_level);
  virtual bool SetBrightnessLevel(int64 level, base::TimeDelta interval);

 private:
  // Generate FilePaths within |dir_path| for reading and writing brightness
  // information.
  static void GetBacklightFilePaths(const FilePath& dir_path,
                                    FilePath* actual_brightness_path,
                                    FilePath* brightness_path,
                                    FilePath* max_brightness_path);

  // Look for the existence of required files and return the max brightness.
  // Returns 0 if necessary files are missing.
  static int64 CheckBacklightFiles(const FilePath& dir_path);

  // Read the value from |path| to |level|.  Returns false on failure.
  static bool ReadBrightnessLevelFromFile(const FilePath& path, int64* level);

  // Writes |level| to |path|.  Returns false on failure.
  static bool WriteBrightnessLevelToFile(const FilePath& path, int64 level);

  // Returns the current time (or |current_time_for_testing_| if non-null).
  base::TimeTicks GetCurrentTime() const;

  // Sets the brightness level appropriately for the current point in the
  // transition.  When the transition is done, clears |transition_timeout_id_|
  // and returns FALSE to cancel the timeout.
  SIGNAL_CALLBACK_0(InternalBacklight, gboolean, HandleTransitionTimeout);

  // Cancels |transition_timeout_id_| if set.
  void CancelTransition();

  // Paths to the actual_brightness, brightness, and max_brightness files
  // under /sys/class/backlight.
  FilePath actual_brightness_path_;
  FilePath brightness_path_;
  FilePath max_brightness_path_;

  // Cached maximum and last-set brightness levels.
  int64 max_brightness_level_;
  int64 current_brightness_level_;

  // GLib source ID for calling HandleTransitionTimeoutThunk().  0 if unset.
  guint transition_timeout_id_;

  // Times at which the current transition started and is scheduled to end.
  base::TimeTicks transition_start_time_;
  base::TimeTicks transition_end_time_;

  // Start and end brightness level for the current transition.
  int64 transition_start_level_;
  int64 transition_end_level_;

  // If non-null, used in place of base::TimeTicks::Now() when the current time
  // is needed.
  base::TimeTicks current_time_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(InternalBacklight);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INTERNAL_BACKLIGHT_H_

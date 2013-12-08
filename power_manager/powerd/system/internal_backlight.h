// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_INTERNAL_BACKLIGHT_H_
#define POWER_MANAGER_POWERD_SYSTEM_INTERNAL_BACKLIGHT_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/memory/scoped_ptr.h"
#include "power_manager/powerd/system/backlight_interface.h"

namespace power_manager {

class Clock;

namespace system {

// Controls an LCD or keyboard backlight via sysfs.
class InternalBacklight : public BacklightInterface {
 public:
  // Base names of backlight files within sysfs directories.
  static const char kBrightnessFilename[];
  static const char kMaxBrightnessFilename[];
  static const char kActualBrightnessFilename[];
  static const char kResumeBrightnessFilename[];

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
  bool Init(const base::FilePath& base_path,
            const base::FilePath::StringType& pattern);

  bool transition_timer_is_running() const {
    return transition_timer_.IsRunning();
  }
  Clock* clock() { return clock_.get(); }

  // Calls HandleTransitionTimeout() as if |transition_timer_| had fired
  // and returns true if the timer is still running afterward and false if it
  // isn't.
  bool TriggerTransitionTimeoutForTesting();

  // Overridden from BacklightInterface:
  virtual int64 GetMaxBrightnessLevel() OVERRIDE;
  virtual int64 GetCurrentBrightnessLevel() OVERRIDE;
  virtual bool SetBrightnessLevel(int64 level, base::TimeDelta interval)
      OVERRIDE;
  virtual bool SetResumeBrightnessLevel(int64 level) OVERRIDE;

 private:
  // Generates FilePaths within |dir_path| for reading and writing
  // brightness information.
  static void GetBacklightFilePaths(const base::FilePath& dir_path,
                                    base::FilePath* actual_brightness_path,
                                    base::FilePath* brightness_path,
                                    base::FilePath* max_brightness_path,
                                    base::FilePath* resume_brightness_path);

  // Looks for the existence of required files and returns the max brightness.
  // Returns 0 if necessary files are missing.
  static int64 CheckBacklightFiles(const base::FilePath& dir_path);

  // Reads the value from |path| to |level|.  Returns false on failure.
  static bool ReadBrightnessLevelFromFile(const base::FilePath& path,
                                          int64* level);

  // Writes |level| to |path|.  Returns false on failure.
  static bool WriteBrightnessLevelToFile(const base::FilePath& path,
                                         int64 level);

  // Sets the brightness level appropriately for the current point in the
  // transition.  When the transition is done, stops |transition_timer_|.
  void HandleTransitionTimeout();

  // Cancels |transition_timeout_id_| if set.
  void CancelTransition();

  scoped_ptr<Clock> clock_;

  // Paths to the actual_brightness, brightness, max_brightness and
  // resume_brightness files under /sys/class/backlight.
  base::FilePath actual_brightness_path_;
  base::FilePath brightness_path_;
  base::FilePath max_brightness_path_;
  base::FilePath resume_brightness_path_;

  // Cached maximum and last-set brightness levels.
  int64 max_brightness_level_;
  int64 current_brightness_level_;

  // Calls HandleTransitionTimeout().
  base::RepeatingTimer<InternalBacklight> transition_timer_;

  // Times at which the current transition started and is scheduled to end.
  base::TimeTicks transition_start_time_;
  base::TimeTicks transition_end_time_;

  // Start and end brightness level for the current transition.
  int64 transition_start_level_;
  int64 transition_end_level_;

  DISALLOW_COPY_AND_ASSIGN(InternalBacklight);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_INTERNAL_BACKLIGHT_H_

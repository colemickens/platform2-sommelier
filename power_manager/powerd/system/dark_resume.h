// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_H_
#define POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/time/time.h>
#include <base/files/file_path.h>

#include "power_manager/powerd/system/power_supply.h"

namespace power_manager {

class PrefsInterface;

namespace system {

// Returns information related to "dark resume", a mode where the system briefly
// resumes from suspend to check the battery level and possibly shut down
// automatically.
class DarkResumeInterface {
 public:
  enum Action {
    // Suspend the system.
    SUSPEND = 0,
    // Shut the system down immediately.
    SHUT_DOWN,
  };

  DarkResumeInterface() {}
  virtual ~DarkResumeInterface() {}

  // Updates state in anticipation of the system suspending, returning the
  // action that should be performed. If SUSPEND is returned, |suspend_duration|
  // contains the duration for which the system should be suspended or an empty
  // base::TimeDelta() if it should be suspended indefinitely.
  virtual void PrepareForSuspendAttempt(Action* action,
                                        base::TimeDelta* suspend_duration) = 0;

  // Returns true if the system is currently in dark resume.
  virtual bool InDarkResume() = 0;
};

// Real implementation of DarkResumeInterface that interacts with sysfs.
class DarkResume : public DarkResumeInterface {
 public:
  // Within a device directory, kPowerDir contains kActiveFile and kSourceFile.
  static const char kPowerDir[];
  static const char kActiveFile[];
  static const char kSourceFile[];

  // Strings to write to sysfs files to enable/disable dark resume functionality
  // at the kernel level.
  static const char kEnabled[];
  static const char kDisabled[];

  DarkResume();
  virtual ~DarkResume();

  void set_dark_resume_state_path_for_testing(const base::FilePath& path) {
    dark_resume_state_path_ = path;
  }

  // Reads preferences on how long to suspend, what devices are affected by
  // suspend, and what devices can wake the system up from suspend.
  // Ownership of passed-in pointers remains with the caller.
  void Init(PowerSupplyInterface* power_supply, PrefsInterface* prefs);

  // DarkResumeInterface implementation:
  void PrepareForSuspendAttempt(Action* action,
                                base::TimeDelta* suspend_duration) override;
  bool InDarkResume() override;

 private:
  // Fills |suspend_durations_|, returning false if the pref was unset or empty.
  bool ReadSuspendDurationsPref();

  // This enables functionality for dark resume in devices that are listed in
  // the prefs_file. The base_file is the name of the sysfs file we are writing
  // to to enable the functionality for dark resume. This includes whether the
  // device should do something different during a dark resume or whether it is
  // a wakeup source for dark resume.
  void GetFiles(std::vector<base::FilePath>* files,
                const std::string& pref_name,
                const std::string& base_file);

  // Writes the passed-in state to all the files in |files|.
  void SetStates(const std::vector<base::FilePath>& files, bool enabled);

  // Is dark resume enabled?
  bool enabled_;

  PowerSupplyInterface* power_supply_;
  PrefsInterface* prefs_;

  // File read to get the dark resume state.
  base::FilePath dark_resume_state_path_;

  // Battery percentage threshold at which the system should shut down after a
  // dark resume.
  double battery_shutdown_threshold_;

  // How long the system should suspend (values) at a given battery percentage
  // (keys).
  typedef std::map<double, base::TimeDelta> SuspendMap;
  SuspendMap suspend_durations_;

  std::vector<base::FilePath> dark_resume_sources_;
  std::vector<base::FilePath> dark_resume_devices_;

  DISALLOW_COPY_AND_ASSIGN(DarkResume);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DARK_RESUME_H_

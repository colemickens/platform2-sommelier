// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_STUB_H_

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "power_manager/powerd/system/backlight_interface.h"

namespace power_manager {
namespace system {

// Stub implementation of BacklightInterface for testing.
class BacklightStub : public BacklightInterface {
 public:
  BacklightStub(int64 max_level, int64 current_level);
  virtual ~BacklightStub();

  void set_max_level(int64 level) { max_level_ = level; }
  void set_current_level(int64 level) { current_level_ = level; }
  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }
  void clear_resume_level() { resume_level_ = -1; }

  int64 current_level() const { return current_level_; }
  int64 resume_level() const { return resume_level_; }
  base::TimeDelta current_interval() const { return current_interval_; }

  // Calls all observers' OnBacklightDeviceChanged() methods.
  void NotifyObservers();

  // BacklightInterface implementation:
  virtual void AddObserver(BacklightInterfaceObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BacklightInterfaceObserver* observer) OVERRIDE;
  virtual bool GetMaxBrightnessLevel(int64* max_level) OVERRIDE;
  virtual bool GetCurrentBrightnessLevel(int64* current_level) OVERRIDE;
  virtual bool SetBrightnessLevel(int64 level, base::TimeDelta interval)
      OVERRIDE;
  virtual bool SetResumeBrightnessLevel(int64 level) OVERRIDE;

 private:
  ObserverList<BacklightInterfaceObserver> observers_;

  // Maximum backlight level.
  int64 max_level_;

  // Most-recently-set brightness level.
  int64 current_level_;

  // Most-recently-set resume level.
  int64 resume_level_;

  // |interval| parameter passed to most recent SetBrightnessLevel() call.
  base::TimeDelta current_interval_;

  // Should we report failure in response to future requests?
  bool should_fail_;

  DISALLOW_COPY_AND_ASSIGN(BacklightStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_STUB_H_

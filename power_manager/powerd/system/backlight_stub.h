// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_STUB_H_
#define POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_STUB_H_

#include <base/basictypes.h>
#include <base/compiler_specific.h>
#include <base/time/time.h>

#include "power_manager/powerd/system/backlight_interface.h"

namespace power_manager {
namespace system {

// Stub implementation of BacklightInterface for testing.
class BacklightStub : public BacklightInterface {
 public:
  BacklightStub(int64_t max_level, int64_t current_level);
  virtual ~BacklightStub();

  void set_max_level(int64_t level) { max_level_ = level; }
  void set_current_level(int64_t level) { current_level_ = level; }
  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }
  void clear_resume_level() { resume_level_ = -1; }

  int64_t current_level() const { return current_level_; }
  int64_t resume_level() const { return resume_level_; }
  base::TimeDelta current_interval() const { return current_interval_; }

  // BacklightInterface implementation:
  virtual int64_t GetMaxBrightnessLevel() OVERRIDE;
  virtual int64_t GetCurrentBrightnessLevel() OVERRIDE;
  virtual bool SetBrightnessLevel(int64_t level, base::TimeDelta interval)
      OVERRIDE;
  virtual bool SetResumeBrightnessLevel(int64_t level) OVERRIDE;

 private:
  // Maximum backlight level.
  int64_t max_level_;

  // Most-recently-set brightness level.
  int64_t current_level_;

  // Most-recently-set resume level.
  int64_t resume_level_;

  // |interval| parameter passed to most recent SetBrightnessLevel() call.
  base::TimeDelta current_interval_;

  // Should we report failure in response to future requests?
  bool should_fail_;

  DISALLOW_COPY_AND_ASSIGN(BacklightStub);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_BACKLIGHT_STUB_H_

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_AUDIO_DETECTOR_H_
#define POWER_MANAGER_AUDIO_DETECTOR_H_

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/time.h"
#include "power_manager/activity_detector_interface.h"
#include "power_manager/signal_callback.h"

namespace power_manager {

typedef int gboolean;
typedef unsigned int guint;

class AudioDetector : public ActivityDetectorInterface {
 public:
  AudioDetector();
  virtual ~AudioDetector() {}

  void Init();

  // Overridden from ActivityDetectorInterface.
  virtual bool GetActivity(int64 activity_threshold_ms,
                           int64* time_since_activity_ms,
                           bool* is_active);
  virtual bool Enable();
  virtual bool Disable();

 private:
  SIGNAL_CALLBACK_0(AudioDetector, gboolean, Poll);
  bool GetAudioStatus(bool* is_active);

  FilePath audio_status_path_;
  base::Time last_audio_time_;
  bool polling_enabled_;
  guint poll_loop_id_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetector);
};

}  // namespace power_manager

#endif  // POWER_MANAGER_AUDIO_DETECTOR_H_

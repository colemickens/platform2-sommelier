// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_AUDIO_DETECTOR_H_
#define POWER_MANAGER_AUDIO_DETECTOR_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "power_manager/activity_detector_interface.h"
#include "power_manager/async_file_reader.h"
#include "power_manager/signal_callback.h"

namespace power_manager {

typedef int gboolean;
typedef unsigned int guint;

class AudioDetector : public ActivityDetectorInterface {
 public:
  AudioDetector();
  virtual ~AudioDetector();

  // Doesn't do anything right now, but keep it as a placeholder to be
  // consistent with VideoDetector.
  void Init();

  // Overridden from ActivityDetectorInterface.
  virtual bool GetActivity(int64 activity_threshold_ms,
                           int64* time_since_activity_ms,
                           bool* is_active);
  virtual void Enable();
  virtual void Disable();

 private:
  // For polling audio status.
  SIGNAL_CALLBACK_0(AudioDetector, gboolean, Poll);

  // Asynchronous I/O success and error handlers, respectively.
  void ReadCallback(const std::string& data);
  void ErrorCallback();

  // Indicates whether an audio status file has been opened for polling, and the
  // polling flag is set.
  bool IsPollingEnabled() const;

  // Timestamp of the last time audio was detected.
  base::Time last_audio_time_;

  // Flag that enables or disables audio detection polling.
  bool polling_enabled_;

  // For asynchronous file access.
  scoped_ptr<AsyncFileReader> audio_file_;

  // Callbacks for asynchronous file I/O.
  base::Callback<void(const std::string&)> read_cb_;
  base::Callback<void()> error_cb_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetector);
};

}  // namespace power_manager

#endif // POWER_MANAGER_AUDIO_DETECTOR_H_

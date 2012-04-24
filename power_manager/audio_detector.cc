// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/audio_detector.h"

#include <gdk/gdkx.h>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"

namespace {

const char kAudioBasePath[] = "/proc/asound/card0/pcm0p";
const int kPollMs = 1000;

}  // namespace

namespace power_manager {

AudioDetector::AudioDetector() : polling_enabled_(false),
                                 poll_loop_id_(0) {}

void AudioDetector::Init() {
  // TODO(sque): We can make this more flexible to accommodate different sysfs.
  FilePath base_path(kAudioBasePath);
  audio_status_path_ = base_path.Append("sub0/status");
}

bool AudioDetector::GetActivity(int64 activity_threshold_ms,
                                int64* time_since_activity_ms,
                                bool* is_active) {
  CHECK(time_since_activity_ms);
  CHECK(is_active);
  *time_since_activity_ms = 0;
  *is_active = false;
  if (!last_audio_time_.is_null()) {
    *time_since_activity_ms =
        (base::Time::Now() - last_audio_time_).InMilliseconds();
    *is_active = *time_since_activity_ms < activity_threshold_ms;
  }
  return true;
}

bool AudioDetector::Enable() {
  if (polling_enabled_)
    return true;
  polling_enabled_ = true;
  poll_loop_id_ = g_timeout_add(kPollMs, PollThunk, this);
  Poll();
  return true;
}

bool AudioDetector::Disable() {
  polling_enabled_ = false;
  if (poll_loop_id_ > 0)
    g_source_remove(poll_loop_id_);
  poll_loop_id_ = 0;
  return true;
}

bool AudioDetector::GetAudioStatus(bool* is_active) {
  CHECK(is_active != NULL);
  if (!file_util::PathExists(audio_status_path_))
    return false;

  // Read and parse the contents of the audio status file.
  std::string sound_buf;
  bool ok = file_util::ReadFileToString(audio_status_path_, &sound_buf);
  if (strstr(sound_buf.c_str(), "closed"))  // Audio is inactive.
    *is_active = false;
  else if (strstr(sound_buf.c_str(), "state: RUNNING"))  // Audio is active.
    *is_active = true;
  else  // Could not determine audio state.
    ok = false;

  return ok;
}

gboolean AudioDetector::Poll() {
  // Do not update the audio poll results if it has been disabled.  Also, do not
  // continue polling.
  if (!polling_enabled_) {
    LOG(WARNING) << "Audio polling should be disabled.";
    return FALSE;
  }
  bool audio_is_playing = false;
  if (!GetAudioStatus(&audio_is_playing)) {
    LOG(ERROR) << "Could not read audio.\n";
    return TRUE;
  }
  // Record audio time.
  if (audio_is_playing)
    last_audio_time_ = base::Time::Now();
  return TRUE;
}

}  // namespace power_manager

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/audio_detector.h"

#include <gdk/gdkx.h>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"

namespace {

// Audio status file path.
const char kAudioStatusPath[] = "/proc/asound/card0/pcm0p/sub0/status";

// Audio polling interval.
const int kPollMs = 1000;

}  // namespace

namespace power_manager {

AudioDetector::AudioDetector()
    : polling_enabled_(false),
      read_cb_(
          base::Bind(&AudioDetector::ReadCallback,base::Unretained(this))),
      error_cb_(
          base::Bind(&AudioDetector::ErrorCallback, base::Unretained(this))) {}

AudioDetector::~AudioDetector() {}

void AudioDetector::Init() {
  if (!audio_file_.Init(kAudioStatusPath))
    LOG(WARNING) << "No audio status file found, audio detection will be "
                 << "disabled.";
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
  if (IsPollingEnabled())
    return true;
  polling_enabled_ = true;
  // Attempt to open the audio file if it's not open already.
  if (!audio_file_.HasOpenedFile())
    Init();
  if (IsPollingEnabled()) {
    Poll();
    return true;
  }
  return false;
}

bool AudioDetector::Disable() {
  polling_enabled_ = false;
  return true;
}

gboolean AudioDetector::Poll() {
  if (!IsPollingEnabled()) {
    LOG(ERROR) << "Polling not enabled or audio status file not found.";
    return FALSE;
  }

  audio_file_.StartRead(&read_cb_, &error_cb_);
  return FALSE;
}

void AudioDetector::ReadCallback(const std::string& data) {
  if (data.find("state: RUNNING") != std::string::npos)
    last_audio_time_ = base::Time::Now();
  // If the polling has been disabled, do not read again.
  if (IsPollingEnabled())
    g_timeout_add(kPollMs, PollThunk, this);
}

void AudioDetector::ErrorCallback() {
  LOG(ERROR) << "Error reading file " << kAudioStatusPath;
  // If the polling has been disabled, do not read again.
  if (IsPollingEnabled())
    g_timeout_add(kPollMs, PollThunk, this);
}

bool AudioDetector::IsPollingEnabled() const {
  return polling_enabled_ && audio_file_.HasOpenedFile();
}

}  // namespace power_manager

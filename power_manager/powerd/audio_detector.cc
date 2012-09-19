// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/audio_detector.h"

#include <glib.h>

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

void AudioDetector::Init() {}

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

void AudioDetector::Enable() {
  if (IsPollingEnabled())
    return;
  polling_enabled_ = true;
  Poll();
}

void AudioDetector::Disable() {
  polling_enabled_ = false;
}

gboolean AudioDetector::Poll() {
  if (!polling_enabled_) {
    LOG(ERROR) << "Polling not enabled.";
    return FALSE;
  }
  // Close and reopen the audio status file because when the status changes, the
  // handle becomes stale.  This is a temporary measure to deal with this quirk
  // until we use cras for audio detection.
  audio_file_.reset(new AsyncFileReader);
  if (!audio_file_->Init(kAudioStatusPath)) {
    LOG(WARNING) << "Audio status file not found, continuing to poll for it.";
    return TRUE;
  }
  audio_file_->StartRead(&read_cb_, &error_cb_);
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
  return polling_enabled_;
}

}  // namespace power_manager

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/audio_detector.h"

#include <cras_client.h>
#include <glib.h>

#include "power_manager/common/util.h"
#include "power_manager/powerd/system/audio_observer.h"

namespace power_manager {
namespace system {

namespace {

// Delay before retrying connecting to Chrome OS audio server.
const int kRetryConnectToCrasMs = 1000;

// Frequency with which audio activity should be checked, in milliseconds.
const int kPollForActivityMs = 5000;

}  // namespace

AudioDetector::AudioDetector()
    : cras_client_(NULL),
      connected_to_cras_(false),
      retry_connect_to_cras_timeout_id_(0),
      poll_for_activity_timeout_id_(0) {
}

AudioDetector::~AudioDetector() {
  util::RemoveTimeout(&retry_connect_to_cras_timeout_id_);
  util::RemoveTimeout(&poll_for_activity_timeout_id_);
  if (cras_client_) {
    if (connected_to_cras_)
      cras_client_stop(cras_client_);
    cras_client_destroy(cras_client_);
  }
}

void AudioDetector::Init(const std::string& headphone_device) {
  headphone_device_ = headphone_device;
  if (!ConnectToCras()) {
    retry_connect_to_cras_timeout_id_ =
        g_timeout_add(kRetryConnectToCrasMs, RetryConnectToCrasThunk, this);
  }
}

void AudioDetector::AddObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AudioDetector::RemoveObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool AudioDetector::IsHeadphoneJackConnected() {
  return connected_to_cras_ &&
      !headphone_device_.empty() &&
      cras_client_output_dev_plugged(cras_client_, headphone_device_.c_str());
}

bool AudioDetector::GetLastAudioActivityTime(base::TimeTicks* time_out) {
  if (!connected_to_cras_) {
    LOG(WARNING) << "Not connected to CRAS";
    return false;
  }

  struct timespec last_audio_time;
  if (cras_client_get_num_active_streams(cras_client_, &last_audio_time) > 0) {
    *time_out = base::TimeTicks::Now();
    return true;
  }

  if (last_audio_time.tv_sec == 0 && last_audio_time.tv_nsec == 0) {
    *time_out = base::TimeTicks();
    return true;
  }

  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now)) {
    LOG(ERROR) << "Could not read current clock time";
    return false;
  }

  int64 delta_seconds = now.tv_sec - last_audio_time.tv_sec;
  int64 delta_ns = now.tv_nsec - last_audio_time.tv_nsec;
  base::TimeDelta last_audio_time_delta =
      base::TimeDelta::FromSeconds(delta_seconds) +
      base::TimeDelta::FromMicroseconds(
          delta_ns / base::Time::kNanosecondsPerMicrosecond);

  if (last_audio_time_delta < base::TimeDelta()) {
    LOG(ERROR) << "Got last-audio time in the future";
    return false;
  }

  *time_out = base::TimeTicks::Now() - last_audio_time_delta;
  return true;
}

bool AudioDetector::ConnectToCras() {
  if (connected_to_cras_)
    return true;

  if (!cras_client_ && cras_client_create(&cras_client_)) {
    LOG(ERROR) << "Couldn't create CRAS client";
    cras_client_ = NULL;
    return false;
  }

  if (cras_client_connect(cras_client_) ||
      cras_client_run_thread(cras_client_)) {
    LOG(WARNING) << "CRAS client couldn't connect to server";
    return false;
  }

  LOG(INFO) << "CRAS client successfully connected to server";
  connected_to_cras_ = true;

  DCHECK(!poll_for_activity_timeout_id_);
  poll_for_activity_timeout_id_ =
      g_timeout_add(kPollForActivityMs, PollForActivityThunk, this);

  return true;
}

gboolean AudioDetector::RetryConnectToCras() {
  if (ConnectToCras()) {
    // If we managed to connect successfully, return FALSE to cancel the
    // GLib timeout.
    retry_connect_to_cras_timeout_id_ = 0;
    return FALSE;
  }
  return TRUE;
}

gboolean AudioDetector::PollForActivity() {
  base::TimeTicks last_activity_time;
  if (GetLastAudioActivityTime(&last_activity_time)) {
    base::TimeDelta time_since_activity =
        base::TimeTicks::Now() - last_activity_time;
    if (time_since_activity.InMilliseconds() <= kPollForActivityMs) {
      FOR_EACH_OBSERVER(AudioObserver, observers_,
                        OnAudioActivity(last_activity_time));
    }
  }
  return TRUE;
}

}  // namespace system
}  // namespace power_manager

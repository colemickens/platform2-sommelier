// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/audio_client.h"

#include <cras_client.h>
#include <glib.h>

#include "power_manager/common/util.h"
#include "power_manager/powerd/system/audio_observer.h"

namespace power_manager {
namespace system {

namespace {

// Delay before retrying connecting to Chrome OS audio server.
const int kRetryConnectToCrasMs = 1000;

// Number of failed connection attempts to CRAS before AudioClient will start
// logging warnings. powerd and CRAS start in parallel, so this helps avoid
// spamming the logs unnecessarily at boot.
const int kNumConnectionAttemptsBeforeLogging = 10;

// Frequency with which audio activity should be checked, in milliseconds.
const int kPollForActivityMs = 5000;

// Types assigned to headphone and HDMI nodes by CRAS.
const char kHeadphoneNodeType[] = "HEADPHONE";
const char kHdmiNodeType[] = "HDMI";

}  // namespace

AudioClient::AudioClient()
    : cras_client_(NULL),
      connected_to_cras_(false),
      num_connection_attempts_(0),
      headphone_plugged_(false),
      hdmi_active_(false),
      mute_stored_(false),
      originally_muted_(false),
      retry_connect_to_cras_timeout_id_(0),
      poll_for_activity_timeout_id_(0) {
}

AudioClient::~AudioClient() {
  util::RemoveTimeout(&retry_connect_to_cras_timeout_id_);
  util::RemoveTimeout(&poll_for_activity_timeout_id_);
  if (cras_client_) {
    if (connected_to_cras_)
      cras_client_stop(cras_client_);
    cras_client_destroy(cras_client_);
  }
}

void AudioClient::Init() {
  if (!ConnectToCras()) {
    retry_connect_to_cras_timeout_id_ =
        g_timeout_add(kRetryConnectToCrasMs, RetryConnectToCrasThunk, this);
  }
}

void AudioClient::AddObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AudioClient::RemoveObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool AudioClient::GetLastAudioActivityTime(base::TimeTicks* time_out) {
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

void AudioClient::MuteSystem() {
  if (!connected_to_cras_ || mute_stored_)
    return;

  mute_stored_ = true;
  originally_muted_ = cras_client_get_system_muted(cras_client_);
  cras_client_set_system_mute(cras_client_, true);
}

void AudioClient::RestoreMutedState() {
  if (!connected_to_cras_ || !mute_stored_)
    return;

  mute_stored_ = false;
  cras_client_set_system_mute(cras_client_, originally_muted_);
}

void AudioClient::UpdateDevices() {
  if (!connected_to_cras_)
    return;

  cras_ionode_info nodes[CRAS_MAX_IONODES];
  size_t num_devs = 0;
  size_t num_nodes = arraysize(nodes);
  if (cras_client_get_output_devices(
          cras_client_, NULL, nodes, &num_devs, &num_nodes) != 0) {
    LOG(WARNING) << "cras_client_get_output_devices() failed";
    return;
  }

  headphone_plugged_ = false;
  hdmi_active_ = false;

  for (size_t i = 0; i < num_nodes; ++i) {
    const cras_ionode_info& node = nodes[i];
    VLOG(1) << "Node " << i << ":"
            << " type=" << node.type
            << " name=" << node.name
            << " plugged=" << node.plugged
            << " active=" << node.active;
    if (strcmp(node.type, kHeadphoneNodeType) == 0 && node.plugged)
      headphone_plugged_ = true;
    else if (strcmp(node.type, kHdmiNodeType) == 0 && node.active)
      hdmi_active_ = true;
  }

  LOG(INFO) << "Updated audio devices: "
            << "headphones " << (headphone_plugged_ ? "" : "un") << "plugged, "
            << "HDMI " << (hdmi_active_ ? "" : "in") << "active";
}

bool AudioClient::ConnectToCras() {
  if (connected_to_cras_)
    return true;

  if (!cras_client_ && cras_client_create(&cras_client_) != 0) {
    LOG(ERROR) << "Couldn't create CRAS client";
    cras_client_ = NULL;
    return false;
  }

  num_connection_attempts_++;
  if (cras_client_connect(cras_client_) != 0 ||
      cras_client_run_thread(cras_client_) != 0) {
    if (num_connection_attempts_ > kNumConnectionAttemptsBeforeLogging)
      LOG(WARNING) << "CRAS client couldn't connect to server";
    return false;
  }

  LOG(INFO) << "Waiting for connection to CRAS server";
  if (cras_client_connected_wait(cras_client_) != 0) {
    LOG(WARNING) << "cras_client_connected_wait() failed";
    return false;
  }

  LOG(INFO) << "CRAS client successfully connected to server";
  connected_to_cras_ = true;

  DCHECK(!poll_for_activity_timeout_id_);
  poll_for_activity_timeout_id_ =
      g_timeout_add(kPollForActivityMs, PollForActivityThunk, this);
  UpdateDevices();

  return true;
}

gboolean AudioClient::RetryConnectToCras() {
  if (ConnectToCras()) {
    // If we managed to connect successfully, return FALSE to cancel the
    // GLib timeout.
    retry_connect_to_cras_timeout_id_ = 0;
    return FALSE;
  }
  return TRUE;
}

gboolean AudioClient::PollForActivity() {
  // TODO(derat): Once CRAS emits D-Bus signals in response to
  // active-stream-count changes, listen for them instead of polling:
  // http://crbug.com/280712
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

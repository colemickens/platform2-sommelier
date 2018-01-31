// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/audio_client_stub.h"

namespace power_manager {
namespace system {

AudioClientStub::AudioClientStub()
    : headphone_jack_plugged_(false),
      hdmi_active_(false),
      suspended_(false),
      initial_loads_(0),
      device_updates_(0),
      stream_updates_(0) {}

AudioClientStub::~AudioClientStub() {}

void AudioClientStub::ResetStats() {
  initial_loads_ = 0;
  device_updates_ = 0;
  stream_updates_ = 0;
}

bool AudioClientStub::GetHeadphoneJackPlugged() const {
  return headphone_jack_plugged_;
}

bool AudioClientStub::GetHdmiActive() const {
  return hdmi_active_;
}

void AudioClientStub::AddObserver(AudioObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void AudioClientStub::RemoveObserver(AudioObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

void AudioClientStub::LoadInitialState() {
  initial_loads_++;
}

void AudioClientStub::UpdateDevices() {
  device_updates_++;
}

void AudioClientStub::UpdateNumOutputStreams() {
  stream_updates_++;
}

void AudioClientStub::SetSuspended(bool suspended) {
  suspended_ = suspended;
}

}  // namespace system
}  // namespace power_manager

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/audio_client.h"

#include <algorithm>
#include <string>

#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>

#include "power_manager/common/util.h"
#include "power_manager/powerd/system/audio_observer.h"

namespace power_manager {
namespace system {

namespace {

// Maximum amount of time to wait for a reply from CRAS, in milliseconds.
const int kCrasDBusTimeoutMs = 3000;

// Keys within node dictionaries returned by CRAS.
const char kTypeKey[] = "Type";
const char kActiveKey[] = "Active";

// Types assigned to headphone and HDMI nodes by CRAS.
const char kHeadphoneNodeType[] = "HEADPHONE";
const char kHdmiNodeType[] = "HDMI";

}  // namespace

AudioClient::AudioClient()
    : cras_proxy_(NULL),
      num_active_streams_(0),
      headphone_jack_plugged_(false),
      hdmi_active_(false),
      mute_stored_(false),
      originally_muted_(false) {
}

AudioClient::~AudioClient() {
}

void AudioClient::Init(dbus::ObjectProxy* cras_proxy) {
  DCHECK(cras_proxy);
  cras_proxy_ = cras_proxy;
}

void AudioClient::AddObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AudioClient::RemoveObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AudioClient::MuteSystem() {
  if (mute_stored_)
    return;

  dbus::MethodCall method_call(cras::kCrasControlInterface,
                               cras::kGetVolumeState);
  scoped_ptr<dbus::Response> response(
      cras_proxy_->CallMethodAndBlock(&method_call, kCrasDBusTimeoutMs));
  if (response) {
    int output_volume = 0;
    bool output_mute = false;
    dbus::MessageReader reader(response.get());
    if (reader.PopInt32(&output_volume) && reader.PopBool(&output_mute)) {
      originally_muted_ = output_mute;
      mute_stored_ = true;
    } else {
      LOG(WARNING) << "Unable to read " << cras::kGetVolumeState << " args";
    }
  }

  if (mute_stored_) {
    VLOG(1) << "Muting system; old state was " << originally_muted_;
    SetOutputMute(true);
  }
}

void AudioClient::RestoreMutedState() {
  if (!mute_stored_)
    return;

  VLOG(1) << "Restoring system mute state to " << originally_muted_;
  SetOutputMute(originally_muted_);
  mute_stored_ = false;
}

void AudioClient::LoadInitialState() {
  UpdateDevices();
  UpdateNumActiveStreams();
}

void AudioClient::UpdateDevices() {
  const bool old_headphone_jack_plugged = headphone_jack_plugged_;
  const bool old_hdmi_active = hdmi_active_;

  headphone_jack_plugged_ = false;
  hdmi_active_ = false;

  dbus::MethodCall method_call(cras::kCrasControlInterface, cras::kGetNodes);
  scoped_ptr<dbus::Response> response(
      cras_proxy_->CallMethodAndBlock(&method_call, kCrasDBusTimeoutMs));
  if (!response)
    return;

  // At the outer level, there's a dictionary corresponding to each audio node.
  dbus::MessageReader response_reader(response.get());
  dbus::MessageReader node_reader(NULL);
  while (response_reader.PopArray(&node_reader)) {
    std::string type;
    bool active = false;

    // Iterate over the dictionary's entries.
    dbus::MessageReader property_reader(NULL);
    while (node_reader.PopDictEntry(&property_reader)) {
      std::string key;
      if (!property_reader.PopString(&key)) {
        LOG(WARNING) << "Skipping dictionary entry with non-string key";
        continue;
      }
      if (key == kTypeKey) {
        if (!property_reader.PopVariantOfString(&type))
          LOG(WARNING) << kTypeKey << " key has non-string value";
      } else if (key == kActiveKey) {
        if (!property_reader.PopVariantOfBool(&active))
          LOG(WARNING) << kActiveKey << " key has non-bool value";
      }
    }

    VLOG(1) << "Saw node: type=" << type << " active=" << active;

    // The D-Bus interface doesn't return unplugged nodes.
    if (type == kHeadphoneNodeType)
      headphone_jack_plugged_ = true;
    else if (type == kHdmiNodeType && active)
      hdmi_active_ = true;
  }

  if (headphone_jack_plugged_ != old_headphone_jack_plugged ||
      hdmi_active_ != old_hdmi_active) {
    LOG(INFO) << "Updated audio devices: headphones "
              << (headphone_jack_plugged_ ? "" : "un") << "plugged, "
              << "HDMI " << (hdmi_active_ ? "" : "in") << "active";
  }
}

void AudioClient::UpdateNumActiveStreams() {
  dbus::MethodCall method_call(cras::kCrasControlInterface,
                               cras::kGetNumberOfActiveStreams);
  scoped_ptr<dbus::Response> response(
      cras_proxy_->CallMethodAndBlock(&method_call, kCrasDBusTimeoutMs));
  int num_streams = 0;
  if (response) {
    dbus::MessageReader reader(response.get());
    if (!reader.PopInt32(&num_streams))
      LOG(WARNING) << "Unable to read " << cras::kGetNumberOfActiveStreams
                   << " args";
  } else {
    LOG(WARNING) << cras::kGetNumberOfActiveStreams << " call failed";
  }

  const int old_num_streams = num_active_streams_;
  num_active_streams_ = std::max(num_streams, 0);

  if (num_active_streams_ && !old_num_streams) {
    LOG(INFO) << "Audio playback started";
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnAudioStateChange(true));
  } else if (!num_active_streams_ && old_num_streams) {
    LOG(INFO) << "Audio playback stopped";
    FOR_EACH_OBSERVER(AudioObserver, observers_, OnAudioStateChange(false));
  }
}

void AudioClient::SetOutputMute(bool mute) {
  dbus::MethodCall method_call(cras::kCrasControlInterface,
                               cras::kSetOutputMute);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(mute);
  scoped_ptr<dbus::Response> response(
      cras_proxy_->CallMethodAndBlock(&method_call, kCrasDBusTimeoutMs));
}

}  // namespace system
}  // namespace power_manager

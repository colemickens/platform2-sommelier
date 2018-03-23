// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/audio_client.h"

#include <algorithm>
#include <memory>
#include <string>

#include <base/bind.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>

#include "power_manager/common/util.h"
#include "power_manager/powerd/system/audio_observer.h"

namespace power_manager {
namespace system {

namespace {

// Maximum amount of time to wait for a reply from CRAS.
constexpr base::TimeDelta kCrasDBusTimeout = base::TimeDelta::FromSeconds(3);

}  // namespace

constexpr char AudioClient::kTypeKey[];
constexpr char AudioClient::kActiveKey[];
constexpr char AudioClient::kHeadphoneNodeType[];
constexpr char AudioClient::kHdmiNodeType[];

AudioClient::AudioClient() : weak_ptr_factory_(this) {}

AudioClient::~AudioClient() {
  if (dbus_wrapper_)
    dbus_wrapper_->RemoveObserver(this);
}

void AudioClient::Init(DBusWrapperInterface* dbus_wrapper) {
  DCHECK(dbus_wrapper);
  dbus_wrapper_ = dbus_wrapper;
  dbus_wrapper_->AddObserver(this);

  cras_proxy_ = dbus_wrapper_->GetObjectProxy(cras::kCrasServiceName,
                                              cras::kCrasServicePath);
  dbus_wrapper_->RegisterForServiceAvailability(
      cras_proxy_, base::Bind(&AudioClient::HandleCrasAvailableOrRestarted,
                              weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper_->RegisterForSignal(
      cras_proxy_, cras::kCrasControlInterface, cras::kNodesChanged,
      base::Bind(&AudioClient::HandleNodesChangedSignal,
                 weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper_->RegisterForSignal(
      cras_proxy_, cras::kCrasControlInterface, cras::kActiveOutputNodeChanged,
      base::Bind(&AudioClient::HandleActiveOutputNodeChangedSignal,
                 weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper_->RegisterForSignal(
      cras_proxy_, cras::kCrasControlInterface,
      cras::kNumberOfActiveStreamsChanged,
      base::Bind(&AudioClient::HandleNumberOfActiveStreamsChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool AudioClient::GetHeadphoneJackPlugged() const {
  return headphone_jack_plugged_;
}

bool AudioClient::GetHdmiActive() const {
  return hdmi_active_;
}

void AudioClient::AddObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AudioClient::RemoveObserver(AudioObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void AudioClient::SetSuspended(bool suspended) {
  dbus::MethodCall method_call(cras::kCrasControlInterface,
                               cras::kSetSuspendAudio);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(suspended);
  dbus_wrapper_->CallMethodSync(cras_proxy_, &method_call, kCrasDBusTimeout);
}

void AudioClient::OnDBusNameOwnerChanged(const std::string& service_name,
                                         const std::string& old_owner,
                                         const std::string& new_owner) {
  if (service_name == cras::kCrasServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << service_name << " ownership changed to "
              << new_owner;
    HandleCrasAvailableOrRestarted(true);
  }
}

void AudioClient::UpdateDevices() {
  const bool old_headphone_jack_plugged = headphone_jack_plugged_;
  const bool old_hdmi_active = hdmi_active_;

  headphone_jack_plugged_ = false;
  hdmi_active_ = false;

  dbus::MethodCall method_call(cras::kCrasControlInterface, cras::kGetNodes);
  std::unique_ptr<dbus::Response> response = dbus_wrapper_->CallMethodSync(
      cras_proxy_, &method_call, kCrasDBusTimeout);
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

void AudioClient::UpdateNumOutputStreams() {
  dbus::MethodCall method_call(cras::kCrasControlInterface,
                               cras::kGetNumberOfActiveOutputStreams);
  std::unique_ptr<dbus::Response> response = dbus_wrapper_->CallMethodSync(
      cras_proxy_, &method_call, kCrasDBusTimeout);
  int num_streams = 0;
  if (response) {
    dbus::MessageReader reader(response.get());
    if (!reader.PopInt32(&num_streams))
      LOG(WARNING) << "Unable to read " << cras::kGetNumberOfActiveOutputStreams
                   << " args";
  } else {
    LOG(WARNING) << cras::kGetNumberOfActiveOutputStreams << " call failed";
  }

  const int old_num_streams = num_output_streams_;
  num_output_streams_ = std::max(num_streams, 0);

  if (num_output_streams_ && !old_num_streams) {
    VLOG(1) << "Audio playback started";
    for (AudioObserver& observer : observers_)
      observer.OnAudioStateChange(true);
  } else if (!num_output_streams_ && old_num_streams) {
    VLOG(1) << "Audio playback stopped";
    for (AudioObserver& observer : observers_)
      observer.OnAudioStateChange(false);
  }
}

void AudioClient::HandleCrasAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for CRAS to become available";
    return;
  }
  UpdateDevices();
  UpdateNumOutputStreams();
}

void AudioClient::HandleNodesChangedSignal(dbus::Signal* signal) {
  UpdateDevices();
}

void AudioClient::HandleActiveOutputNodeChangedSignal(dbus::Signal* signal) {
  UpdateDevices();
}

void AudioClient::HandleNumberOfActiveStreamsChanged(dbus::Signal* signal) {
  UpdateNumOutputStreams();
}

}  // namespace system
}  // namespace power_manager

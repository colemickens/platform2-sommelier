// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_
#define POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

#include <base/macros.h>
#include <base/observer_list.h>
#include <dbus/object_proxy.h>

namespace power_manager {
namespace system {

class AudioObserver;

// AudioClient monitors audio activity as reported by CRAS, the Chrome OS
// audio server.
class AudioClient {
 public:
  AudioClient();
  ~AudioClient();

  bool headphone_jack_plugged() const { return headphone_jack_plugged_; }
  bool hdmi_active() const { return hdmi_active_; }

  // Initializes the object. |cras_proxy| is used to communicate with CRAS.
  void Init(dbus::ObjectProxy* cras_proxy);

  // Adds or removes an observer.
  void AddObserver(AudioObserver* observer);
  void RemoveObserver(AudioObserver* observer);

  // Mutes the system.
  void MuteSystem();

  // Restores the muted state the system had before the call to MuteSystem.
  // Multiple calls to MuteSystem do not stack.
  void RestoreMutedState();

  // Calls Update*() to load the initial state from CRAS.
  void LoadInitialState();

  // Updates the client's view of connected audio devices.
  void UpdateDevices();

  // Updates |num_active_streams_| and notifies observers if the state changed.
  void UpdateNumActiveStreams();

 private:
  // Sends a request to CRAS asking it to mute or unmute the system volume.
  void SetOutputMute(bool mute);

  dbus::ObjectProxy* cras_proxy_;  // weak

  // Number of audio streams (either input or output) currently active.
  int num_active_streams_;

  // Is something plugged in to a headphone jack?
  bool headphone_jack_plugged_;

  // Is an HDMI output active?
  bool hdmi_active_;

  // Indicates whether the muted state was successfully stored by a call to
  // MuteSystem().
  bool mute_stored_;

  // The state the system was in before the call to MuteSystem().
  bool originally_muted_;

  ObserverList<AudioObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioClient);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_
#define POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

#include <base/macros.h>
#include <base/observer_list.h>

#include "power_manager/powerd/system/audio_client_interface.h"

namespace dbus {
class ObjectProxy;
}  // namespace dbus

namespace power_manager {
namespace system {

class DBusWrapperInterface;

// Real implementation of AudioClientInterface that monitors audio activity as
// reported by CRAS, the Chrome OS audio server.
// TODO(derat): Write unit tests for this class now that it's using
// DBusWrapperInterface.
class AudioClient : public AudioClientInterface {
 public:
  AudioClient();
  ~AudioClient() override;

  // Initializes the object. Ownership of |dbus_wrapper| remains with the
  // caller.
  void Init(DBusWrapperInterface* dbus_wrapper);

  // AudioClientInterface:
  bool GetHeadphoneJackPlugged() const override;
  bool GetHdmiActive() const override;
  void AddObserver(AudioObserver* observer) override;
  void RemoveObserver(AudioObserver* observer) override;
  void SetSuspended(bool suspended) override;
  void LoadInitialState() override;
  void UpdateDevices() override;
  void UpdateNumOutputStreams() override;

 private:
  DBusWrapperInterface* dbus_wrapper_ = nullptr;  // weak
  dbus::ObjectProxy* cras_proxy_ = nullptr;       // weak

  // Number of audio output streams currently active.
  int num_output_streams_ = 0;

  // Is something plugged in to a headphone jack?
  bool headphone_jack_plugged_ = false;

  // Is an HDMI output active?
  bool hdmi_active_ = false;

  base::ObserverList<AudioObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioClient);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

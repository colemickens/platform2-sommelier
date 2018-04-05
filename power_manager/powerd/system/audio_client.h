// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_
#define POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

#include "power_manager/powerd/system/audio_client_interface.h"

#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>
#include <base/observer_list.h>

#include "power_manager/powerd/system/dbus_wrapper.h"

namespace dbus {
class ObjectProxy;
class Response;
class Signal;
}  // namespace dbus

namespace power_manager {
namespace system {

// Real implementation of AudioClientInterface that monitors audio activity as
// reported by CRAS, the Chrome OS audio server.
class AudioClient : public AudioClientInterface,
                    public DBusWrapperInterface::Observer {
 public:
  // Keys within node dictionaries returned by CRAS.
  static constexpr char kTypeKey[] = "Type";
  static constexpr char kActiveKey[] = "Active";

  // Types assigned to headphone and HDMI nodes by CRAS.
  static constexpr char kHeadphoneNodeType[] = "HEADPHONE";
  static constexpr char kHdmiNodeType[] = "HDMI";

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

  // DBusWrapperInterface::Observer:
  void OnDBusNameOwnerChanged(const std::string& service_name,
                              const std::string& old_owner,
                              const std::string& new_owner) override;

 private:
  // Asynchronously updates |headphone_jack_plugged_| and |hdmi_active_|.
  void CallGetNodes();
  void HandleGetNodesResponse(dbus::Response* response);

  // Asynchronously updates |num_output_streams_| and notifies observers if the
  // state changed.
  void CallGetNumberOfActiveOutputStreams();
  void HandleGetNumberOfActiveOutputStreamsResponse(dbus::Response* response);

  // Handles various events announced over D-Bus.
  void HandleCrasAvailableOrRestarted(bool available);
  void HandleNodesChangedSignal(dbus::Signal* signal);
  void HandleActiveOutputNodeChangedSignal(dbus::Signal* signal);
  void HandleNumberOfActiveStreamsChangedSignal(dbus::Signal* signal);

  DBusWrapperInterface* dbus_wrapper_ = nullptr;  // weak
  dbus::ObjectProxy* cras_proxy_ = nullptr;       // weak

  // Number of audio output streams currently open.
  int num_output_streams_ = 0;

  // Is something plugged in to a headphone jack?
  bool headphone_jack_plugged_ = false;

  // Is an HDMI output active?
  bool hdmi_active_ = false;

  base::ObserverList<AudioObserver> observers_;

  base::WeakPtrFactory<AudioClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AudioClient);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

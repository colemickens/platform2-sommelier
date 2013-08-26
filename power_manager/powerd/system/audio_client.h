// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_
#define POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "power_manager/common/signal_callback.h"

typedef int gboolean;
typedef unsigned int guint;

struct cras_client;

namespace power_manager {
namespace system {

class AudioObserver;

// AudioClient monitors audio activity as reported by CRAS, the Chrome OS
// audio server.
class AudioClient {
 public:
  AudioClient();
  ~AudioClient();

  bool headphone_plugged() const { return headphone_plugged_; }
  bool hdmi_active() const { return hdmi_active_; }

  // Starts attempting to connect to CRAS.  Note that the connection may
  // happen asynchronously if the server is initially unavailable.
  void Init();

  // Adds or removes an observer.
  void AddObserver(AudioObserver* observer);
  void RemoveObserver(AudioObserver* observer);

  // Updates |time_out| to contain the time at which CRAS reports that
  // audio was last played or recorded.  If no audio activity has been
  // observed, an empty base::TimeTicks will be used.  Returns false on
  // failure.
  bool GetLastAudioActivityTime(base::TimeTicks* time_out);

  // Mutes the system.
  void MuteSystem();

  // Restores the muted state the system had before the call to MuteSystem.
  // Multiple calls to MuteSystem do not stack.
  void RestoreMutedState();

  // Updates the client's view of connected audio devices.
  void UpdateDevices();

 private:
  // Attempts to connect to the CRAS server, allocating |cras_client_| if
  // needed.  Starts |poll_for_activity_timeout_id_| on success.  Returns
  // false if unable to connect.
  bool ConnectToCras();

  // Invoked by |retry_connect_to_cras_timeout_id_|.  Calls ConnectToCras()
  // and continues or stops the timeout as needed.
  SIGNAL_CALLBACK_0(AudioClient, gboolean, RetryConnectToCras);

  SIGNAL_CALLBACK_0(AudioClient, gboolean, PollForActivity);

  // Client object for communicating with the server component of CRAS.
  cras_client* cras_client_;

  // Indicates whether |cras_client_| is initialized and connected to the
  // server.
  bool connected_to_cras_;

  // Number of attempts to connect to the CRAS server made so far.
  int num_connection_attempts_;

  // Is something plugged in to a headphone jack?
  bool headphone_plugged_;

  // Is an HDMI output active?
  bool hdmi_active_;

  // Indicates whether the muted state was successfully stored by a call to
  // MuteSystem().
  bool mute_stored_;

  // The state the system was in before the call to MuteSystem().
  bool originally_muted_;

  // GLib timeout ID for running RetryConnectToCras(), or 0 if unset.
  guint retry_connect_to_cras_timeout_id_;

  // GLib timeout ID for running PollForActivity(), or 0 if unset.
  guint poll_for_activity_timeout_id_;

  ObserverList<AudioObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AudioClient);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_H_

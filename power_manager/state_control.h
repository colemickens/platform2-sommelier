// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STATE_CONTROL_H_
#define STATE_CONTROL_H_

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include <map>

#include "power_manager/power_prefs.h"
#include "power_manager/signal_callback.h"

namespace power_manager {

class Daemon;

struct StateControlInfo {
  unsigned int request_id;
  unsigned int duration;
  time_t expires;
  bool disable_idle_dim;
  bool disable_idle_blank;
  bool disable_idle_suspend;
  bool disable_lid_suspend;
};

enum StateControlStates { kIdleDimDisabled, kIdleBlankDisabled,
                          kIdleSuspendDisabled, kLidSuspendDisabled };


// StateControl is used to manage requests from external sources to
// disable parts of the state machine temporarily.  Applications send
// a protobuf through dbus to powerd which then calls
// StateControl::StateOverrideRequest().
// Within the powerd state machine, it queries for disabled states via
// StateControl::IsStateDisabled()
// Requests to disable the state machine will either time out after their
// duration has passed (default value of 30 minutes, controllable via
// power prefs file state_max_disabled_duration_sec) or a request is
// explicitly canceled through StateControl::RemoveOverride()
class StateControl {
 public:
  explicit StateControl(Daemon* daemon = NULL);
  virtual ~StateControl();

  void RemoveOverride(int request_id);
  void RemoveOverrideAndUpdate(int request_id);
  bool StateOverrideRequest(const char* data, const int size,
                            int* return_value);
  bool StateOverrideRequestStruct(const StateControlInfo* request,
                                  int* return_value);
  bool IsStateDisabled(StateControlStates state);
  void ReadSettings(PowerPrefs* prefs);

 private:
  friend class StateControlTest;
  FRIEND_TEST(StateControlTest, InvalidRequests);
  FRIEND_TEST(StateControlTest, InterleavedRequests);
  FRIEND_TEST(StateControlTest, SingleRequests);
  FRIEND_TEST(StateControlTest, TimingRequests);
  FRIEND_TEST(StateControlTest, WrapTest);

  void DumpInfoRec(const StateControlInfo* info);
  void RescanState(time_t cur_time = 0);
  SIGNAL_CALLBACK_0(StateControl, gboolean, RecordExpired);

  typedef std::map<unsigned int, StateControlInfo*> StateControlList;
  StateControlList state_override_list_;
  unsigned int last_id_;
  time_t next_check_;
  unsigned int max_duration_;

  bool disable_idle_dim_;
  bool disable_idle_blank_;
  bool disable_idle_suspend_;
  bool disable_lid_suspend_;

  Daemon* daemon_;  // The powerd daemon.  Pointer owned by powerd.
};

}  // namespace power_manager

#endif  // STATE_CONTROL_H_

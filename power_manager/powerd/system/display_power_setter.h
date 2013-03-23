// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_DISPLAY_POWER_SETTER_H_
#define POWER_MANAGER_POWERD_SYSTEM_DISPLAY_POWER_SETTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "chromeos/dbus/service_constants.h"
#include "power_manager/common/signal_callback.h"

typedef int gboolean;
typedef unsigned int guint;

namespace power_manager {
namespace system {

// Interface for turning displays on and off.
class DisplayPowerSetterInterface {
 public:
  DisplayPowerSetterInterface() {}
  virtual ~DisplayPowerSetterInterface() {}

  // Configures displays to use |state| after |delay|.  If another change
  // has already been scheduled, it will be aborted.  Note that even with
  // an empty delay, the change may be applied asynchronously.
  virtual void SetDisplayPower(chromeos::DisplayPowerState state,
                               base::TimeDelta delay) = 0;

  // Tells Chrome to simulate the display being dimmed or undimmed in
  // software.  This is used as a substitute for actually changing the
  // display's brightness in some cases, e.g. for external displays.
  virtual void SetDisplaySoftwareDimming(bool dimmed) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayPowerSetterInterface);
};

// Real DisplayPowerSetterInterface implementation that makes D-Bus method
// calls to Chrome.
class DisplayPowerSetter : public DisplayPowerSetterInterface {
 public:
  DisplayPowerSetter();
  virtual ~DisplayPowerSetter();

  // DisplayPowerSetterInterface implementation:
  virtual void SetDisplayPower(chromeos::DisplayPowerState state,
                               base::TimeDelta delay) OVERRIDE;
  virtual void SetDisplaySoftwareDimming(bool dimmed) OVERRIDE;

 private:
  // Makes an asynchronous D-Bus method call to Chrome to apply |state|.
  void SendStateToChrome(chromeos::DisplayPowerState state);

  // Timeout callback that sets the passed-in state, resets |timeout_id_|,
  // and returns FALSE to cancel the timeout.
  SIGNAL_CALLBACK_PACKED_1(DisplayPowerSetter,
                           gboolean,
                           HandleTimeout,
                           chromeos::DisplayPowerState);

  // GLib source ID for timeout that will run HandleTimeout(), or 0 if no
  // timeout is currently set.
  guint timeout_id_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPowerSetter);
};

}  // system
}  // power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_DISPLAY_POWER_SETTER_H_

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_ARC_TIMER_MANAGER_H_
#define POWER_MANAGER_POWERD_SYSTEM_ARC_TIMER_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "power_manager/powerd/system/dbus_wrapper.h"

namespace dbus {
class MessageReader;
class MethodCall;
class ObjectProxy;
class Response;
}  // namespace dbus

namespace power_manager {
namespace system {

class ArcTimerManager {
 public:
  ArcTimerManager();
  ~ArcTimerManager();

  // Initializes the D-Bus API handlers.
  void Init(DBusWrapperInterface* dbus_wrapper);

 private:
  // Metadata associated with a timer set for the instance.
  struct ArcTimerInfo;

  // Receives an array of |int32_t clock_id, base::ScopedFD expiration_fd|.
  // Clears all previous timers and creates an |ArcTimerInfo| entry for each
  // |clock_id|. Returns true iff timers corresponding to all clocks in the
  // arguments are created. Only one timer per clock is allowed, returns false
  // if the same clock is present more than once in the arguments.
  void HandleCreateArcTimers(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Receives |int32_t clock_id, base::TimeTicks absolute_expiration_time| over
  // D-Bus. Starts the timer of type |clock_id| to run at
  // |absolute_expiration_time| in the future. If the timer is already running,
  // it will be replaced. Notification will be performed as an 8-byte write to
  // the associated expiration fd. Returns true if the timer can be started and
  // false otherwise.
  void HandleStartArcTimer(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Deletes all |ArcTimerInfo| entries and stops any pending timers. Returns an
  // empty response in all cases.
  void HandleDeleteArcTimers(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Creates |ArcTimerInfo| by parsing |clock_id, expiration_fd| at the current
  // position in |array_reader|. Returns null on failure i.e. invalid arguments
  // in |array_reader| or failure while allocating resources. Returns non-null
  // iff |ArcTimerInfo| object is created successfully.
  std::unique_ptr<ArcTimerInfo> CreateArcTimer(
      dbus::MessageReader* array_reader);

  // Finds |ArcTimerInfo| entry in |arc_timers_| corresponding to
  // |clock_id|. Returns null if an entry is not present. Returns non-null
  // pointer otherwise.
  ArcTimerInfo* FindArcTimerInfo(int32_t clock_id);

  // Map that stores |ArcTimerInfo|s corresponding to different clocks used by
  // the instance. Each clock type has only one timer associated with it.
  std::map<int32_t, std::unique_ptr<ArcTimerInfo>> arc_timers_;

  base::WeakPtrFactory<ArcTimerManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcTimerManager);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_ARC_TIMER_MANAGER_H_

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_ARC_TIMER_MANAGER_H_
#define POWER_MANAGER_POWERD_SYSTEM_ARC_TIMER_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

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

  using TimerId = int32_t;

  // Initializes the D-Bus API handlers.
  void Init(DBusWrapperInterface* dbus_wrapper);

 private:
  // Metadata associated with a timer set for the instance.
  struct ArcTimerInfo;

  // Monotonically increasing timer id that will be associated with each managed
  // timer. Returned from |CreateArcTimers| and used by callers to refer to
  // their timer. Always >= 1.
  TimerId next_timer_id_ = 1;

  // Receives a |string tag, array of
  // |int32_t clock_id, base::ScopedFD expiration_fd|. Creates an |ArcTimerInfo|
  // entry for each |clock_id|. Returns a D-Bus response with an |array of
  // int32_t timer_ids| iff timers corresponding to all clocks in the arguments
  // are created. Only one timer per clock is allowed, returns B-Bus error if
  // the same clock is present more than once in the arguments.Also, returns
  // D-Bus error if timers corresponding to the client's tag already exist. In
  // such a scenario the client must delete timers associated with it's tag and
  // then create them again.
  void HandleCreateArcTimers(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Creates a vector of |ArcTimerInfo|s by parsing |clock_id, expiration_fd| at
  // the current position in |array_reader|. Returns an empty vector on failure
  // i.e. invalid arguments in |array_reader| or failure while allocating
  // resources. Returns a non-empty vector iff |ArcTimerInfo| objects are
  // created successfully.
  static std::vector<std::unique_ptr<ArcTimerInfo>> CreateArcTimers(
      dbus::MessageReader* array_reader);

  // Creates |ArcTimerInfo| by parsing |clock_id, expiration_fd| at the current
  // position in |array_reader|. Returns null on failure i.e. invalid arguments
  // in |array_reader| or failure while allocating resources. Returns non-null
  // iff |ArcTimerInfo| object is created successfully.
  static std::unique_ptr<ArcTimerInfo> CreateArcTimer(
      dbus::MessageReader* array_reader);

  // Returns true iff |arc_timers| have duplicate clock ids between two or more
  // entries.
  // Note: Is static because it needs to access |ArcTimerInfo|.
  static bool ContainsDuplicateClocks(
      const std::vector<std::unique_ptr<ArcTimerInfo>>& arc_timers);

  // Receives |int32_t timer_id, base::TimeTicks absolute_expiration_time| over
  // D-Bus. Starts the timer of id |timer_id| to run at
  // |absolute_expiration_time| in the future. If the timer is already running,
  // it will be replaced. Notification will be performed as an 8-byte write to
  // the associated expiration fd. Returns D-Bus error if |timer_id| is not
  // present in the stored timers. Returns empty D-Bus response iff timer is
  // started successfully.
  void HandleStartArcTimer(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Receives a |string tag| as an argument. Deletes all |ArcTimerInfo| entries
  // associated with the client's tag in |timers_| and stops any pending timers.
  // Also, deletes client's entry in |client_timer_ids_|. Returns a D-Bus error
  // if the tag is found and any associated timers and metadata can't be
  // deleted. Returns an empty D-Bus response if no timers exist corresponding
  // to the client's tag or if a client's associated timers and metadata is
  // deleted successfully.
  void HandleDeleteArcTimers(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Finds |ArcTimerInfo| entry in |timers_| corresponding to |timer_id|.
  // Returns non-null pointer iff entry is present.
  ArcTimerInfo* FindArcTimerInfo(TimerId timer_id);

  // Map that stores |ArcTimerInfo|s corresponding to different timer ids.
  std::map<TimerId, std::unique_ptr<ArcTimerInfo>> timers_;

  // List of timer ids associated with a client's tag i.e. the timer ids
  // being used by each client.
  std::map<std::string, std::vector<TimerId>> client_timer_ids_;

  base::WeakPtrFactory<ArcTimerManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcTimerManager);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_ARC_TIMER_MANAGER_H_

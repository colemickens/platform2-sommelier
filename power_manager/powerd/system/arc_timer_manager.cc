// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/arc_timer_manager.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/files/file.h>
#include <base/memory/ptr_util.h>
#include <base/posix/unix_domain_socket_linux.h>
#include <brillo/daemons/daemon.h>
#include <components/timers/alarm_timer_chromeos.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>

namespace {

// Creates a new "invalid args" reply to |method_call|.
std::unique_ptr<dbus::Response> CreateInvalidArgsError(
    dbus::MethodCall* method_call, const std::string& message) {
  return std::unique_ptr<dbus::Response>(dbus::ErrorResponse::FromMethodCall(
      method_call, DBUS_ERROR_INVALID_ARGS, message));
}

// Only wake up alarms are supported.
bool IsSupportedClock(int32_t clock_id) {
  return clock_id == CLOCK_BOOTTIME_ALARM || clock_id == CLOCK_REALTIME_ALARM;
}

// Expiration callback for timer of type |clock_id|. |expiration_fd| is the fd
// to write to to indicate timer expiration to the instance.
void OnExpiration(int32_t clock_id, int expiration_fd) {
  DVLOG(1) << "Expiration callback for clock=" << clock_id;
  // Write to the |expiration_fd| to indicate to the instance that the timer has
  // expired. The instance expects 8 bytes on the read end similar to what
  // happens on a timerfd expiration. The timerfd API expects this to be the
  // number of expirations, however, more than one expiration isn't tracked
  // currently. This can block in the unlikely scenario of multiple writes
  // happening but the instance not reading the data. When the send queue is
  // full (64Kb), a write attempt here will block.
  const uint64_t timer_data = 1;
  if (!base::UnixDomainSocket::SendMsg(
          expiration_fd, &timer_data, sizeof(timer_data), std::vector<int>())) {
    PLOG(ERROR) << "Failed to indicate timer expiration to the instance";
  }
}

// TODO(abhishekbh): Copied from Chrome's //base/time/time_now_posix.cc. Make
// upstream code available via libchrome and use it here:
// http://crbug.com/166153.
int64_t ConvertTimespecToMicros(const struct timespec& ts) {
  // On 32-bit systems, the calculation cannot overflow int64_t.
  // 2**32 * 1000000 + 2**64 / 1000 < 2**63
  if (sizeof(ts.tv_sec) <= 4 && sizeof(ts.tv_nsec) <= 8) {
    int64_t result = ts.tv_sec;
    result *= base::Time::kMicrosecondsPerSecond;
    result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
    return result;
  } else {
    base::CheckedNumeric<int64_t> result(ts.tv_sec);
    result *= base::Time::kMicrosecondsPerSecond;
    result += (ts.tv_nsec / base::Time::kNanosecondsPerMicrosecond);
    return result.ValueOrDie();
  }
}

// TODO(abhishekbh): Copied from Chrome's //base/time/time_now_posix.cc. Make
// upstream code available via libchrome and use it here:
// http://crbug.com/166153.
// Returns count of |clk_id|. Retuns 0 if |clk_id| isn't present on the system.
int64_t ClockNow(clockid_t clk_id) {
  struct timespec ts;
  if (clock_gettime(clk_id, &ts) != 0) {
    NOTREACHED() << "clock_gettime(" << clk_id << ") failed.";
    return 0;
  }
  return ConvertTimespecToMicros(ts);
}

// TODO(abhishekbh): Make this available in upstream Chrome as tracked by
// http://crbug.com/166153.
// Returns the amount of ticks at the time of invocation including ticks spent
// in sleep.
base::TimeTicks GetCurrentBootTicks() {
  return base::TimeTicks() +
         base::TimeDelta::FromMicroseconds(ClockNow(CLOCK_BOOTTIME));
}

}  // namespace

namespace power_manager {
namespace system {

ArcTimerManager::ArcTimerManager() : weak_ptr_factory_(this) {}
ArcTimerManager::~ArcTimerManager() = default;

struct ArcTimerManager::ArcTimerInfo {
  ArcTimerInfo() = default;
  ArcTimerInfo(ArcTimerInfo&&) = default;
  ArcTimerInfo(int32_t clock_id,
               base::ScopedFD expiration_fd,
               std::unique_ptr<timers::SimpleAlarmTimer> timer)
      : clock_id(clock_id),
        expiration_fd(std::move(expiration_fd)),
        timer(std::move(timer)) {}

  // Clock id associated with this timer.
  const int32_t clock_id;

  // The file descriptor which will be written to when |timer| expires.
  const base::ScopedFD expiration_fd;

  // The timer that will be scheduled.
  const std::unique_ptr<timers::SimpleAlarmTimer> timer;

  DISALLOW_COPY_AND_ASSIGN(ArcTimerInfo);
};

void ArcTimerManager::Init(DBusWrapperInterface* dbus_wrapper) {
  DCHECK(dbus_wrapper);
  dbus_wrapper->ExportMethod(kCreateArcTimersMethod,
                             base::Bind(&ArcTimerManager::HandleCreateArcTimers,
                                        weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper->ExportMethod(kStartArcTimerMethod,
                             base::Bind(&ArcTimerManager::HandleStartArcTimer,
                                        weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper->ExportMethod(kDeleteArcTimersMethod,
                             base::Bind(&ArcTimerManager::HandleDeleteArcTimers,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ArcTimerManager::HandleCreateArcTimers(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(1) << "CreateArcTimers";
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageReader reader(method_call);

  dbus::MessageReader array_reader(nullptr);
  if (!reader.PopArray(&array_reader)) {
    LOG(ERROR) << "Failed to pop array";
    response_sender.Run(CreateInvalidArgsError(
        method_call, "Expected array of clock ids and and expiration fds"));
    return;
  }

  // Cancel all previous timers and clean up open descriptors. This is required
  // if the instance goes down and comes back up again.
  arc_timers_.clear();

  // Iterate over the array of |clock_id, expiration_fd| and create an
  // |ArcTimerInfo| entry for each clock.
  while (array_reader.HasMoreData()) {
    std::unique_ptr<ArcTimerInfo> arc_timer = CreateArcTimer(&array_reader);
    if (!arc_timer) {
      // Clear any set up timers in case of error.
      arc_timers_.clear();
      response_sender.Run(CreateInvalidArgsError(
          method_call, "Invalid clock id or expiration fd"));
      return;
    }
    arc_timers_.emplace(arc_timer->clock_id, std::move(arc_timer));
  }
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void ArcTimerManager::HandleStartArcTimer(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  dbus::MessageReader reader(method_call);

  int32_t clock_id;
  if (!reader.PopInt32(&clock_id)) {
    LOG(ERROR) << "Failed to pop clock id";
    response_sender.Run(
        CreateInvalidArgsError(method_call, "Expected clock id"));
    return;
  }

  int64_t absolute_expiration_time_us;
  if (!reader.PopInt64(&absolute_expiration_time_us)) {
    LOG(ERROR) << "Failed to pop absolute expiration time";
    response_sender.Run(CreateInvalidArgsError(
        method_call, "Expected absolute expiration time"));
    return;
  }
  base::TimeTicks absolute_expiration_time =
      base::TimeTicks() +
      base::TimeDelta::FromMicroseconds(absolute_expiration_time_us);

  // If a timer for the given clock is not created prior to this call then
  // return error. Else retrieve the timer associated with it.
  ArcTimerInfo* arc_timer = FindArcTimerInfo(clock_id);
  if (!arc_timer) {
    response_sender.Run(CreateInvalidArgsError(
        method_call, "Invalid clock " + std::to_string(clock_id)));
    return;
  }

  // Start the timer to expire at |absolute_expiration_time|. This call
  // automatically overrides the previous timer set.
  //
  // If the firing time has expired then set the timer to expire
  // immediately. The |current_time_ticks| should always include ticks spent
  // in sleep.
  base::TimeTicks current_time_ticks = GetCurrentBootTicks();
  base::TimeDelta delay;
  if (absolute_expiration_time > current_time_ticks)
    delay = absolute_expiration_time - current_time_ticks;
  base::Time current_time = base::Time::Now();
  DVLOG(1) << "CurrentTime: " << current_time
           << " NextAlarmAt: " << current_time + delay;
  // Pass the raw fd to write to when the timer expires. This is safe to do
  // because if the parent object goes away the timers are cleared and all
  // pending callbacks are cancelled. If the instance sets new timers after a
  // respawn, again, the old timers and pending callbacks are cancelled.
  // TODO(abhishekbh): This needs to be base::BindRepeating but it's not
  // available in the current version of libchrome.
  arc_timer->timer->Start(
      FROM_HERE, delay,
      base::Bind(&OnExpiration, clock_id, arc_timer->expiration_fd.get()));
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

void ArcTimerManager::HandleDeleteArcTimers(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  DVLOG(1) << "DeleteArcTimerInfos";
  arc_timers_.clear();
  response_sender.Run(dbus::Response::FromMethodCall(method_call));
}

std::unique_ptr<ArcTimerManager::ArcTimerInfo> ArcTimerManager::CreateArcTimer(
    dbus::MessageReader* array_reader) {
  dbus::MessageReader struct_reader(nullptr);
  if (!array_reader->PopStruct(&struct_reader)) {
    LOG(ERROR) << "Failed to pop struct";
    return nullptr;
  }

  int32_t clock_id;
  if (!struct_reader.PopInt32(&clock_id)) {
    LOG(ERROR) << "Failed to pop clock id";
    return nullptr;
  }

  // TODO(b/69759087): Make |ArcTimer| take |clock_id| to create timers of
  // different clock types.
  // The instance opens clocks of type CLOCK_BOOTTIME_ALARM and
  // CLOCK_REALTIME_ALARM. However, it uses only CLOCK_BOOTTIME_ALARM to set
  // wake up alarms. At this point, it's okay to pretend the host supports
  // CLOCK_REALTIME_ALARM instead of returning an error.
  //
  // Mojo guarantees to call all callbacks on the task runner that the
  // mojo::Binding i.e. |ArcTimer| was created on.
  if (!IsSupportedClock(clock_id)) {
    LOG(ERROR) << "Unsupported clock " << clock_id;
    return nullptr;
  }

  // Each clock can only have a unique entry.
  if (FindArcTimerInfo(clock_id)) {
    LOG(ERROR) << "Timer already exists for clock " << clock_id;
    return nullptr;
  }

  base::ScopedFD expiration_fd;
  if (!struct_reader.PopFileDescriptor(&expiration_fd)) {
    LOG(ERROR) << "Failed to pop file descriptor for clock " << clock_id;
    return nullptr;
  }
  if (!expiration_fd.is_valid()) {
    LOG(ERROR) << "Bad file descriptor for clock " << clock_id;
    return nullptr;
  }

  return std::make_unique<ArcTimerInfo>(
      clock_id, std::move(expiration_fd),
      std::make_unique<timers::SimpleAlarmTimer>());
}

ArcTimerManager::ArcTimerInfo* ArcTimerManager::FindArcTimerInfo(
    int32_t clock_id) {
  auto it = arc_timers_.find(clock_id);
  return (it == arc_timers_.end()) ? nullptr : it->second.get();
}

}  // namespace system
}  // namespace power_manager

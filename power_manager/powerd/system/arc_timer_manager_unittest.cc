// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/arc_timer_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/callback_helpers.h>
#include <base/macros.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <chromeos/dbus/service_constants.h>
#include <gtest/gtest.h>
#include <sys/socket.h>

#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/system/dbus_wrapper_stub.h"

namespace power_manager {
namespace system {

namespace {

bool CreateSocketPair(base::ScopedFD* one, base::ScopedFD* two) {
  int raw_socks[2];
  if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, raw_socks) != 0) {
    PLOG(ERROR) << "Failed to create socket pair";
    return false;
  }
  one->reset(raw_socks[0]);
  two->reset(raw_socks[1]);
  return true;
}

}  // namespace

class ArcTimerManagerTest : public ::testing::Test,
                            public base::MessageLoopForIO::Watcher {
 public:
  ArcTimerManagerTest() { arc_timer_manager_.Init(&dbus_wrapper_); }

  // base::MessageLoopForIO::Watcher overrides.
  void OnFileCanReadWithoutBlocking(int fd) override {
    VLOG(1) << "Fd readable";
    DCHECK(arc_timer_store_.HasFd(fd));
    // The fd watched in |WaitForExpiration| is now ready to read. Call the quit
    // closure so that |WaitForExpiration| can go on to read the fd.
    loop_runner_.StopLoop();
  }
  void OnFileCanWriteWithoutBlocking(int fd) override {}

 protected:
  bool CreateTimers(const std::vector<int32_t>& clocks) WARN_UNUSED_RESULT {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kCreateArcTimersMethod);
    // Create D-Bus arguments i.e. array of
    // {int32_t clock_id, base::ScopedFD expiration_fd}.
    dbus::MessageWriter writer(&method_call);
    dbus::MessageWriter array_writer(nullptr);
    writer.OpenArray("(ih)", &array_writer);
    for (int32_t clock_id : clocks) {
      // Create a socket pair for each clock. One socket will be part of the
      // mojo argument and will be used by the host to indicate when the timer
      // expires. The other socket will be used to detect the expiration of the
      // timer by epolling / reading.
      base::ScopedFD read_fd;
      base::ScopedFD write_fd;
      if (!CreateSocketPair(&read_fd, &write_fd)) {
        ADD_FAILURE() << "Failed to create socket pair for ARC timers";
        return false;
      }

      // Create an entry for each clock in the local store. This will be used to
      // retrieve the fd to wait on for a timer expiration.
      if (!arc_timer_store_.AddTimer(clock_id, std::move(read_fd))) {
        ADD_FAILURE() << "Failed to create timer entry";
        arc_timer_store_.ClearTimers();
        return false;
      }

      dbus::MessageWriter struct_writer(nullptr);
      array_writer.OpenStruct(&struct_writer);
      struct_writer.AppendInt32(clock_id);
      struct_writer.AppendFileDescriptor(write_fd.get());
      array_writer.CloseContainer(&struct_writer);
    }
    writer.CloseContainer(&array_writer);

    std::unique_ptr<dbus::Response> response =
        dbus_wrapper_.CallExportedMethodSync(&method_call);
    if (!response) {
      ADD_FAILURE() << power_manager::kCreateArcTimersMethod << " call failed";
      arc_timer_store_.ClearTimers();
      return false;
    }
    return true;
  }

  bool StartTimer(int32_t clock_id,
                  base::TimeTicks absolute_expiration_time) WARN_UNUSED_RESULT {
    dbus::MethodCall method_call(power_manager::kPowerManagerInterface,
                                 power_manager::kStartArcTimerMethod);

    // Write clock id and 64-bit expiration time ticks value as a DBus message.
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(clock_id);
    writer.AppendInt64(
        (absolute_expiration_time - base::TimeTicks()).InMicroseconds());
    std::unique_ptr<dbus::Response> response =
        dbus_wrapper_.CallExportedMethodSync(&method_call);
    if (!response) {
      ADD_FAILURE() << power_manager::kStartArcTimerMethod << " call failed";
      arc_timer_store_.ClearTimers();
      return false;
    }
    return true;
  }

  // Returns true iff the read descriptor of a timer is signalled. If the
  // signalling is incorrect returns false. Blocks otherwise.
  bool WaitForExpiration(int32_t clock_id) WARN_UNUSED_RESULT {
    if (!arc_timer_store_.HasTimer(clock_id)) {
      ADD_FAILURE() << "Timer of type: " << clock_id << " not present";
      return false;
    }

    // Wait for the host to indicate expiration by watching the read end of the
    // socket pair.
    int timer_read_fd = arc_timer_store_.GetTimerReadFd(clock_id);
    if (timer_read_fd < 0) {
      ADD_FAILURE() << "Clock " << clock_id << " fd not present";
      return false;
    }

    // Set up a watcher to watch for the timer's read fd to become readable.
    // Make this non-persistent as the callback shouldn't fire multiple times
    // before the data is read.
    base::MessageLoopForIO::FileDescriptorWatcher controller(FROM_HERE);
    if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
            timer_read_fd, false, base::MessageLoopForIO::WATCH_READ,
            &controller, this)) {
      ADD_FAILURE() << "Failed to watch read fd";
      return false;
    }
    // Start run loop and error out if the fd isn't readable after a timeout.
    if (!loop_runner_.StartLoop(base::TimeDelta::FromSeconds(30))) {
      ADD_FAILURE() << "Timed out waiting for expiration";
      return false;
    }

    // The timer expects 8 bytes to be written from the host upon expiration.
    // The read data signifies the number of expirations. The powerd
    // implementation always returns one expiration.
    uint64_t timer_data = 0;
    ssize_t bytes_read = read(timer_read_fd, &timer_data, sizeof(timer_data));
    if ((bytes_read != sizeof(timer_data)) || (timer_data != 1)) {
      ADD_FAILURE() << "Bad expiration data: bytes_read " << bytes_read
                    << " timer_data " << timer_data;
      return false;
    }
    return true;
  }

 private:
  // Stores clock ids and their corresponding file descriptors. These file
  // descriptors indicate when a timer corresponding to the clock has expired on
  // a read.
  class ArcTimerStore {
   public:
    ArcTimerStore() = default;

    bool AddTimer(int32_t clock_id, base::ScopedFD read_fd) {
      if (!arc_timers_.emplace(clock_id, std::move(read_fd)).second)
        ADD_FAILURE() << "Failed to create timer entry";
      return true;
    }

    void ClearTimers() { return arc_timers_.clear(); }

    int GetTimerReadFd(int32_t clock_id) {
      return HasTimer(clock_id) ? arc_timers_[clock_id].get() : -1;
    }

    bool HasTimer(int32_t clock_id) const {
      return arc_timers_.find(clock_id) != arc_timers_.end();
    }

    bool HasFd(int fd) const {
      for (const auto& timer : arc_timers_) {
        if (timer.second.get() == fd)
          return true;
      }
      return false;
    }

   private:
    std::map<int32_t, base::ScopedFD> arc_timers_;

    DISALLOW_COPY_AND_ASSIGN(ArcTimerStore);
  };

  ArcTimerManager arc_timer_manager_;
  ArcTimerStore arc_timer_store_;
  DBusWrapperStub dbus_wrapper_;
  // Run loop used in |WaitForExpiration|. Its |StopLoop| method is called in
  // |OnFileCanReadWithoutBlocking|.
  TestMainLoopRunner loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ArcTimerManagerTest);
};

TEST_F(ArcTimerManagerTest, CreateAndStartTimer) {
  std::vector<int32_t> clocks = {CLOCK_REALTIME_ALARM, CLOCK_BOOTTIME_ALARM};
  VLOG(1) << "Creating timers";
  ASSERT_TRUE(CreateTimers(clocks));
  VLOG(1) << "Starting timer";
  ASSERT_TRUE(StartTimer(
      CLOCK_BOOTTIME_ALARM,
      base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(1)));
  VLOG(1) << "Waiting for expiration";
  ASSERT_TRUE(WaitForExpiration(CLOCK_BOOTTIME_ALARM));
}

}  // namespace system
}  // namespace power_manager

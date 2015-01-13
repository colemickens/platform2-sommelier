// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/asynchronous_signal_handler.h"

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>

namespace {
const int kInvalidDescriptor = -1;
}  // namespace

namespace chromeos {

AsynchronousSignalHandler::AsynchronousSignalHandler()
    : fd_watcher_(new base::MessageLoopForIO::FileDescriptorWatcher),
      descriptor_(kInvalidDescriptor) {
  CHECK_EQ(sigemptyset(&signal_mask_), 0) << "Failed to initialize signal mask";
  CHECK_EQ(sigemptyset(&saved_signal_mask_), 0)
      << "Failed to initialize signal mask";
}

AsynchronousSignalHandler::~AsynchronousSignalHandler() {
  if (descriptor_ != kInvalidDescriptor) {
    fd_watcher_->StopWatchingFileDescriptor();

    if (close(descriptor_) != 0)
      PLOG(WARNING) << "Failed to close file descriptor";

    descriptor_ = kInvalidDescriptor;
    CHECK_EQ(0, sigprocmask(SIG_SETMASK, &saved_signal_mask_, nullptr));
  }
}

void AsynchronousSignalHandler::Init() {
  CHECK_EQ(kInvalidDescriptor, descriptor_);
  CHECK_EQ(0, sigprocmask(SIG_BLOCK, &signal_mask_, &saved_signal_mask_));
  descriptor_ =
      signalfd(descriptor_, &signal_mask_, SFD_CLOEXEC | SFD_NONBLOCK);
  CHECK_NE(kInvalidDescriptor, descriptor_);
  CHECK(base::MessageLoopForIO::current()->WatchFileDescriptor(
      descriptor_,
      true,
      base::MessageLoopForIO::WATCH_READ,
      fd_watcher_.get(),
      this))
      << "Watching shutdown pipe failed.";
}

void AsynchronousSignalHandler::RegisterHandler(int signal,
                                                const SignalHandler& callback) {
  registered_callbacks_[signal] = callback;
  CHECK_EQ(0, sigaddset(&signal_mask_, signal));
  UpdateSignals();
}

void AsynchronousSignalHandler::UnregisterHandler(int signal) {
  Callbacks::iterator callback_it = registered_callbacks_.find(signal);
  if (callback_it != registered_callbacks_.end()) {
    registered_callbacks_.erase(callback_it);
    ResetSignal(signal);
  }
}

void AsynchronousSignalHandler::OnFileCanReadWithoutBlocking(int fd) {
  struct signalfd_siginfo info;
  while (base::ReadFromFD(fd, reinterpret_cast<char*>(&info), sizeof(info))) {
    int signal = info.ssi_signo;
    Callbacks::iterator callback_it = registered_callbacks_.find(signal);
    if (callback_it == registered_callbacks_.end()) {
      LOG(WARNING) << "Unable to find a signal handler for signal: " << signal;
      // Can happen if a signal has been called multiple time, and the callback
      // asked to be unregistered the first time.
      continue;
    }
    const SignalHandler& callback = callback_it->second;
    bool must_unregister = callback.Run(info);
    if (must_unregister) {
      UnregisterHandler(signal);
    }
  }
}

void AsynchronousSignalHandler::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

void AsynchronousSignalHandler::ResetSignal(int signal) {
  CHECK_EQ(0, sigdelset(&signal_mask_, signal));
  UpdateSignals();
}

void AsynchronousSignalHandler::UpdateSignals() {
  if (descriptor_ != kInvalidDescriptor) {
    CHECK_EQ(0, sigprocmask(SIG_SETMASK, &saved_signal_mask_, nullptr));
    CHECK_EQ(0, sigprocmask(SIG_BLOCK, &signal_mask_, nullptr));
    CHECK_EQ(descriptor_,
             signalfd(descriptor_, &signal_mask_, SFD_CLOEXEC | SFD_NONBLOCK));
  }
}

}  // namespace chromeos

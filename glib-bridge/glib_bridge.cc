// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "glib-bridge/glib_bridge.h"

namespace glib_bridge {

namespace {

base::MessageLoopForIO::Mode ConvertGPollFlags(int flags) {
  if ((flags & (G_IO_IN | G_IO_OUT)) == (G_IO_IN | G_IO_OUT))
    return base::MessageLoopForIO::WATCH_READ_WRITE;
  else if ((flags & G_IO_IN) == G_IO_IN)
    return base::MessageLoopForIO::WATCH_READ;
  else if ((flags & G_IO_OUT) == G_IO_OUT)
    return base::MessageLoopForIO::WATCH_WRITE;

  NOTREACHED();
  return base::MessageLoopForIO::WATCH_READ;
}

}  // namespace

struct GMainContextLock {
 public:
  explicit GMainContextLock(GMainContext* context) : context_(context) {
    CHECK(context_);
    CHECK_EQ(g_main_context_acquire(context_), TRUE);
  }

  ~GMainContextLock() { g_main_context_release(context_); }

 private:
  GMainContext* context_;
};

GlibBridge::GlibBridge(base::MessageLoopForIO* message_loop)
    : glib_context_(g_main_context_default()),
      message_loop_(message_loop),
      state_(State::kPreparingIteration),
      weak_ptr_factory_(this) {
  message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GlibBridge::PrepareIteration,
                            weak_ptr_factory_.GetWeakPtr()));
}

void GlibBridge::PrepareIteration() {
  if (state_ != State::kPreparingIteration)
    return;

  GMainContextLock _l(glib_context_);

  g_main_context_prepare(glib_context_, &max_priority_);

  int num_fds =
      g_main_context_query(glib_context_, max_priority_, nullptr, nullptr, 0);
  poll_fds_ = std::vector<GPollFD>(num_fds);
  CHECK(watchers_.empty());

  int timeout_ms;
  g_main_context_query(glib_context_, max_priority_, &timeout_ms, &poll_fds_[0],
                       num_fds);
  DVLOG(1) << "Preparing iteration with timeout " << timeout_ms << " ms, "
           << num_fds << " event FDs";

  for (int i = 0; i < num_fds; i++)
    fd_map_[poll_fds_[i].fd].push_back(&poll_fds_[i]);

  // Collect information about which poll flags we need for each fd.
  std::map<int, int> poll_flags;
  for (const GPollFD& poll_fd : poll_fds_)
    poll_flags[poll_fd.fd] |= poll_fd.events;

  for (const auto& fd_flags : poll_flags) {
    watchers_[fd_flags.first] =
        std::make_unique<base::MessageLoopForIO::FileDescriptorWatcher>(
            FROM_HERE);
    message_loop_->WatchFileDescriptor(fd_flags.first, true,
                                       ConvertGPollFlags(fd_flags.second),
                                       watchers_[fd_flags.first].get(), this);
  }

  state_ = State::kWaitingForEvents;
  if (timeout_ms < 0)
    return;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(timeout_ms);
  timeout_closure_.Reset(
      base::Bind(&GlibBridge::Dispatch, weak_ptr_factory_.GetWeakPtr()));
  message_loop_->task_runner()->PostDelayedTask(
      FROM_HERE, timeout_closure_.callback(), timeout);
}

void GlibBridge::OnEvent(int fd, int flag) {
  for (GPollFD* poll_fd : fd_map_[fd])
    poll_fd->revents |= flag & poll_fd->events;

  watchers_[fd]->StopWatchingFileDescriptor();

  if (state_ != State::kWaitingForEvents)
    return;

  message_loop_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&GlibBridge::Dispatch, weak_ptr_factory_.GetWeakPtr()));
  state_ = State::kReadyForDispatch;
}

void GlibBridge::Dispatch() {
  GMainContextLock _l(glib_context_);

  timeout_closure_.Cancel();
  watchers_.clear();

  if (g_main_context_check(glib_context_, max_priority_, poll_fds_.data(),
                           poll_fds_.size())) {
    g_main_context_dispatch(glib_context_);
  }

  poll_fds_.clear();
  max_priority_ = -1;
  message_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&GlibBridge::PrepareIteration,
                            weak_ptr_factory_.GetWeakPtr()));
  state_ = State::kPreparingIteration;
}

void GlibBridge::OnFileCanWriteWithoutBlocking(int fd) {
  OnEvent(fd, G_IO_OUT);
}

void GlibBridge::OnFileCanReadWithoutBlocking(int fd) {
  OnEvent(fd, G_IO_IN);
}

}  // namespace glib_bridge

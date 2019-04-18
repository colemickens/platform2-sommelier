// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/socket_forwarder.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/time/time.h>

namespace arc_networkd {
namespace {
constexpr int kBufSize = 4096;
// Maximum number of epoll events to process per wait.
constexpr int kMaxEvents = 4;
}  // namespace

SocketForwarder::SocketForwarder(const std::string& name,
                                 std::unique_ptr<Socket> sock0,
                                 std::unique_ptr<Socket> sock1)
    : base::SimpleThread(name),
      sock0_(std::move(sock0)),
      sock1_(std::move(sock1)) {}

SocketForwarder::~SocketForwarder() {
  sock1_.reset();
  sock0_.reset();
  Join();
}

void SocketForwarder::Run() {
  LOG(INFO) << "Starting forwarder: " << *sock0_ << " <-> " << *sock1_;

  // We need these sockets to be non-blocking.
  if (fcntl(sock0_->fd(), F_SETFL,
            fcntl(sock0_->fd(), F_GETFL, 0) | O_NONBLOCK) < 0 ||
      fcntl(sock1_->fd(), F_SETFL,
            fcntl(sock1_->fd(), F_GETFL, 0) | O_NONBLOCK) < 0) {
    PLOG(ERROR) << "fcntl failed";
    return;
  }

  base::ScopedFD efd(epoll_create1(0));
  if (!efd.is_valid()) {
    PLOG(ERROR) << "epoll_create1 failed";
    return;
  }
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLRDHUP;
  ev.data.fd = sock0_->fd();
  if (epoll_ctl(efd.get(), EPOLL_CTL_ADD, sock0_->fd(), &ev) == -1) {
    PLOG(ERROR) << "epoll_ctl failed";
    return;
  }
  ev.data.fd = sock1_->fd();
  if (epoll_ctl(efd.get(), EPOLL_CTL_ADD, sock1_->fd(), &ev) == -1) {
    PLOG(ERROR) << "epoll_ctl failed";
    return;
  }

  struct epoll_event events[kMaxEvents];
  char buf0[kBufSize] = {0}, buf1[kBufSize] = {0};
  ssize_t len0 = 0, len1 = 0;
  bool run = true;
  while (run) {
    int n = epoll_wait(efd.get(), events, kMaxEvents, -1);
    if (n == -1) {
      PLOG(ERROR) << "epoll_wait failed";
      break;
    }
    for (int i = 0; i < n; ++i) {
      if (events[i].events & EPOLLERR) {
        PLOG(WARNING) << "Socket error: " << *sock0_ << " <-> " << *sock1_;
        run = false;
        break;
      }
      if (events[i].events & (EPOLLHUP | EPOLLRDHUP)) {
        LOG(INFO) << "Peer closed connection: " << *sock0_ << " <-> "
                  << *sock1_;
        run = false;
        break;
      }

      if (events[i].events & EPOLLOUT) {
        Socket* dst;
        char* buf;
        ssize_t* len;
        if (sock0_->fd() == events[i].data.fd) {
          dst = sock0_.get();
          buf = buf1;
          len = &len1;
        } else {
          dst = sock1_.get();
          buf = buf0;
          len = &len0;
        }

        ssize_t bytes = dst->SendTo(buf, *len);
        if (bytes < 0) {
          run = false;
          break;
        }
        // Still unavailable.
        if (bytes == 0)
          continue;
        // Partial write.
        if (bytes < *len)
          memmove(&buf[0], &buf[bytes], *len - bytes);
        *len -= bytes;

        if (*len == 0) {
          ev.events = EPOLLIN | EPOLLRDHUP;
          ev.data.fd = dst->fd();
          if (epoll_ctl(efd.get(), EPOLL_CTL_MOD, dst->fd(), &ev) == -1) {
            PLOG(ERROR) << "epoll_ctl failed";
            return;
          }
        }
      }

      if (events[i].events & EPOLLIN) {
        Socket *src, *dst;
        char* buf;
        ssize_t* len;
        if (sock0_->fd() == events[i].data.fd) {
          src = sock0_.get();
          dst = sock1_.get();
          buf = buf0;
          len = &len0;
        } else {
          src = sock1_.get();
          dst = sock0_.get();
          buf = buf1;
          len = &len1;
        }
        // Skip the read if this buffer is still pending write: requires that
        // epoll_wait is in level-triggered mode.
        if (*len > 0) {
          continue;
        }
        *len = src->RecvFrom(buf, kBufSize);
        if (*len < 0) {
          run = false;
          break;
        }
        if (*len == 0) {
          continue;
        }

        ssize_t bytes = dst->SendTo(buf, *len);
        if (bytes < 0) {
          run = false;
          break;
        }
        if (bytes > 0) {
          // Partial write.
          if (bytes < *len)
            memmove(&buf[0], &buf[bytes], *len - bytes);
          *len -= bytes;
        }

        if (*len > 0) {
          ev.events = EPOLLOUT | EPOLLRDHUP;
          ev.data.fd = dst->fd();
          if (epoll_ctl(efd.get(), EPOLL_CTL_MOD, dst->fd(), &ev) == -1) {
            PLOG(ERROR) << "epoll_ctl failed";
            return;
          }
        }
      }
    }
  }

  LOG(INFO) << "Stopping forwarder: " << *sock0_ << " <-> " << *sock1_;
  sock1_.reset();
  sock0_.reset();
}

}  // namespace arc_networkd

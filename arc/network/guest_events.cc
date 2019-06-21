// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/guest_events.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <memory>
#include <vector>

#include <base/logging.h>

#include "arc/network/socket.h"

namespace arc_networkd {
namespace {
constexpr char kMsgFmt[] = "%d %d %d";

bool NotifyArcEvent(const ArcGuestEvent& event) {
  static constexpr size_t kBufSize = 128;
  char buf[kBufSize] = {0};
  if (snprintf(buf, kBufSize, kMsgFmt, event.isVm(), event.isStarting(),
               event.id()) < 0) {
    PLOG(ERROR) << "Cannot create message";
    return false;
  }

  struct sockaddr_un addr = {0};
  socklen_t addrlen = 0;
  FillGuestSocketAddr(&addr, &addrlen);
  Socket gsock(AF_UNIX, SOCK_DGRAM);
  if (gsock.SendTo(buf, strlen(buf), (const struct sockaddr*)&addr, addrlen) <
      0) {
    LOG(ERROR) << "Cannot send message: " << buf;
    return false;
  }

  return true;
}

}  // namespace

ArcGuestEvent::ArcGuestEvent(bool is_vm, bool is_starting, int id)
    : is_vm_(is_vm), is_starting_(is_starting), id_(id) {}

// static
std::unique_ptr<ArcGuestEvent> ArcGuestEvent::Parse(const std::string& msg) {
  if (msg.empty())
    return nullptr;

  int vm = 0, start = 0, id = 0;
  if (sscanf(msg.c_str(), kMsgFmt, &vm, &start, &id) != 3) {
    PLOG(ERROR) << "Cannot parse message: " << msg;
    return nullptr;
  }

  return std::make_unique<ArcGuestEvent>(vm, start, id);
}

void FillGuestSocketAddr(struct sockaddr_un* addr, socklen_t* len) {
  addr->sun_family = AF_UNIX;
  // Start at pos 1 to make this an abstract socket. Note that SUN_LEN does not
  // work in this case since it uses strlen, so this is the correct way to
  // compute the length of addr.
  addr->sun_path[0] = 0;
  strncpy(&addr->sun_path[1], kGuestSocketPath, strlen(kGuestSocketPath));
  *len = offsetof(struct sockaddr_un, sun_path) + strlen(kGuestSocketPath) + 1;
}

bool NotifyArcVmStart(int vsock_cid) {
  return NotifyArcEvent(ArcGuestEvent(true, true, vsock_cid));
}

bool NotifyArcVmStop() {
  return NotifyArcEvent(ArcGuestEvent(true, false, -1));
}

}  // namespace arc_networkd

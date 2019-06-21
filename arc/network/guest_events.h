// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_GUEST_EVENTS_H_
#define ARC_NETWORK_GUEST_EVENTS_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <memory>
#include <string>

#include <base/macros.h>
#include <brillo/brillo_export.h>

namespace arc_networkd {

static constexpr char kGuestSocketPath[] = "/run/arc/network.gsock";

// Simple wrapper class around the guest event message.
// TODO(garrick): Replace with proto.
class ArcGuestEvent {
 public:
  // |id| can be either the container pid or the vsock cid.
  ArcGuestEvent(bool is_vm, bool is_starting, int id);
  ~ArcGuestEvent() = default;

  static std::unique_ptr<ArcGuestEvent> Parse(const std::string& msg);

  bool isVm() const { return is_vm_; }
  bool isStarting() const { return is_starting_; }
  int id() const { return id_; }

 private:
  bool is_vm_;
  bool is_starting_;
  int id_;

  DISALLOW_COPY_AND_ASSIGN(ArcGuestEvent);
};

void FillGuestSocketAddr(struct sockaddr_un* addr, socklen_t* len);

BRILLO_EXPORT bool NotifyArcVmStart(int vsock_cid);
BRILLO_EXPORT bool NotifyArcVmStop();

}  // namespace arc_networkd

#endif  // ARC_NETWORK_GUEST_EVENTS_H_

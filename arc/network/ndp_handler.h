// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_NDP_HANDLER_H_
#define ARC_NETWORK_NDP_HANDLER_H_

#include <ndp.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/macros.h>
#include <base/memory/weak_ptr.h>

namespace arc_networkd {

// Uses libndp to listen for multicast messages of type |msg_type| on
// network interface |ifname|, then calls OnNdpMsg() when one is received.
class NdpHandler {
 public:
  NdpHandler();
  virtual ~NdpHandler();

 protected:
  bool StartNdp(const std::string& ifname, enum ndp_msg_type msg_type);
  void StopNdp();

  virtual int OnNdpMsg(struct ndp* ndp, struct ndp_msg* msg) = 0;

  static const char* MsgTypeName(enum ndp_msg_type msg_type);

  struct ndp* ndp_{nullptr};
  int ifindex_;
  enum ndp_msg_type msg_type_;

  base::WeakPtrFactory<NdpHandler> weak_factory_{this};

 private:
  void OnFileCanReadWithoutBlocking();

  static int LibNdpCallback(struct ndp* ndp, struct ndp_msg* msg, void* priv);

  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher_;
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_NDP_HANDLER_H_

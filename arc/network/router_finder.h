// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_ROUTER_FINDER_H_
#define ARC_NETWORK_ROUTER_FINDER_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/macros.h>

#include "arc/network/ndp_handler.h"

namespace arc_networkd {

// Sends out IPv6 Router Solicitation requests and waits to receive a
// Router Advertisement message.  This is used to perform
// stateless autoconfiguration on behalf of the containerized OS, which
// cannot directly access the host's LAN interface.
class RouterFinder : public NdpHandler {
 public:
  RouterFinder();
  ~RouterFinder() override;

  bool Start(
      const std::string& ifname,
      const base::Callback<void(const struct in6_addr& prefix,
                                int prefix_len,
                                const struct in6_addr& router)>& callback);

 protected:
  // NdpHandler override.
  int OnNdpMsg(struct ndp* ndp, struct ndp_msg* msg) override;

  void SendRouterSolicitation();
  void CheckForRouter();
  void AssignPrefix(const struct in6_addr& prefix,
                    int prefix_len,
                    const struct in6_addr& router);

  std::string ifname_;
  bool have_prefix_;
  struct in6_addr prefix_;
  int prefix_len_;

  unsigned int rs_attempts_;

  base::Callback<void(const struct in6_addr& prefix,
                      int prefix_len,
                      const struct in6_addr& router)>
      result_callback_;

  base::WeakPtrFactory<RouterFinder> weak_factory_{this};
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_ROUTER_FINDER_H_

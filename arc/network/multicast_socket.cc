// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/arc_ip_config.h"
#include "arc/network/multicast_socket.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <utility>

#include <base/logging.h>

namespace arc_networkd {

MulticastSocket::~MulticastSocket() {
  if (fd_.is_valid())
    watcher_.StopWatchingFileDescriptor();
}

bool MulticastSocket::Bind(const std::string& ifname,
                           const struct in_addr& mcast_addr,
                           unsigned short port,
                           MessageLoopForIO::Watcher* parent) {
  CHECK(!fd_.is_valid());

  base::ScopedFD fd(socket(AF_INET, SOCK_DGRAM, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "socket() failed for multicast forwarder on " << ifname
                << " for " << mcast_addr << ":" << port;
    return false;
  }

  // The socket needs to be bound to INADDR_ANY rather than a specific
  // interface, or it will not receive multicast traffic.  Therefore
  // we use SO_BINDTODEVICE to force TX from this interface, and
  // specify the interface address in IP_ADD_MEMBERSHIP to control RX.
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);
  if (setsockopt(fd.get(), SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr))) {
    PLOG(ERROR) << "setsockopt(SOL_SOCKET) failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port;
    return false;
  }

  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));

  if (mcast_addr.s_addr == htonl(INADDR_BROADCAST)) {
    // FIXME: RX needs to be limited to the given interface.
    int on = 1;
    if (setsockopt(fd.get(), SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
      PLOG(ERROR)
          << "setsockopt(SO_BROADCAST) failed for multicast forwarder on "
          << ifname << " for " << mcast_addr << ":" << port;
      return false;
    }
    bind_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  } else {
    int ifindex = if_nametoindex(ifname.c_str());
    if (ifindex == 0) {
      PLOG(ERROR)
          << "could not obtain interface index for multicast forwarder on "
          << ifname << " for " << mcast_addr << ":" << port;
      return false;
    }
    struct ip_mreqn mreqn;
    memset(&mreqn, 0, sizeof(mreqn));
    mreqn.imr_multiaddr = mcast_addr;
    mreqn.imr_address.s_addr = htonl(INADDR_ANY);
    mreqn.imr_ifindex = ifindex;
    if (setsockopt(fd.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn,
                   sizeof(mreqn)) < 0) {
      PLOG(ERROR)
          << "can't add multicast membership for multicast forwarder on "
          << ifname << " for " << mcast_addr << ":" << port;
      return false;
    }
  }

  int off = 0;
  if (setsockopt(fd.get(), IPPROTO_IP, IP_MULTICAST_LOOP, &off, sizeof(off))) {
    PLOG(ERROR)
        << "setsockopt(IP_MULTICAST_LOOP) failed for multicast forwarder on "
        << ifname << " for " << mcast_addr << ":" << port;
    return false;
  }

  int on = 1;
  if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    PLOG(ERROR) << "setsockopt(SO_REUSEADDR) failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port;
    return false;
  }

  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(port);

  if (bind(fd.get(), (const struct sockaddr*)&bind_addr, sizeof(bind_addr)) <
      0) {
    PLOG(ERROR) << "bind(" << port << ") failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port;
    return false;
  }

  MessageLoopForIO::current()->WatchFileDescriptor(
      fd.get(), true, MessageLoopForIO::WATCH_READ, &watcher_, parent);

  fd_ = std::move(fd);
  ifname_ = ifname;
  return true;
}

bool MulticastSocket::SendTo(const void* data,
                             size_t len,
                             const struct sockaddr_in& addr) {
  if (sendto(fd_.get(), data, len, 0,
             reinterpret_cast<const struct sockaddr*>(&addr),
             sizeof(struct sockaddr_in)) < 0) {
    PLOG(WARNING) << "sendto failed";
    return false;
  }
  last_used_ = time(NULL);
  return true;
}

struct in_addr const MulticastSocket::GetInterfaceIp() {
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname_.c_str(), IFNAMSIZ);
  if (ioctl(fd_.get(), SIOCGIFADDR, &ifr) < 0) {
    // Ignore EADDRNOTAVAIL: layer 3 was not provisioned.
    if (errno != EADDRNOTAVAIL) {
      PLOG(ERROR) << "SIOCGIFADDR failed for " << ifname_;
    }
    return {0};
  }

  struct sockaddr_in* if_addr =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  return if_addr->sin_addr;
}

}  // namespace arc_networkd

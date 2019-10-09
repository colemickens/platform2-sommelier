// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/network/multicast_forwarder.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>

#include "arc/network/dns/dns_protocol.h"
#include "arc/network/dns/dns_response.h"
#include "arc/network/net_util.h"
#include "arc/network/socket.h"

namespace {

const int kBufSize = 1536;

// Returns the IPv4 address assigned to the interface on which the given socket
// is bound. Or returns INADDR_ANY if the interface has no IPv4 address.
struct in_addr GetInterfaceIp(int fd, const std::string& ifname) {
  if (ifname.empty()) {
    LOG(WARNING) << "Empty interface name";
    return {0};
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ);
  if (ioctl(fd, SIOCGIFADDR, &ifr) < 0) {
    // Ignore EADDRNOTAVAIL: IPv4 was not provisioned.
    if (errno != EADDRNOTAVAIL) {
      PLOG(ERROR) << "SIOCGIFADDR failed for " << ifname;
    }
    return {0};
  }

  struct sockaddr_in* if_addr =
      reinterpret_cast<struct sockaddr_in*>(&ifr.ifr_addr);
  return if_addr->sin_addr;
}
}  // namespace

namespace arc_networkd {

MulticastForwarder::Socket::Socket(base::ScopedFD fd,
                                   const base::Callback<void(int)>& callback)
    : fd(std::move(fd)) {
  watcher = base::FileDescriptorWatcher::WatchReadable(
      Socket::fd.get(), base::BindRepeating(callback, Socket::fd.get()));
}

base::ScopedFD MulticastForwarder::Bind(const std::string& ifname,
                                        const struct in_addr& mcast_addr,
                                        uint16_t port) {
  base::ScopedFD fd(socket(AF_INET, SOCK_DGRAM, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "socket() failed for multicast forwarder on " << ifname
                << " for " << mcast_addr << ":" << port;
    return base::ScopedFD();
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
    return base::ScopedFD();
  }

  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));

  int ifindex = if_nametoindex(ifname.c_str());
  if (ifindex == 0) {
    PLOG(ERROR)
        << "Could not obtain interface index for multicast forwarder on "
        << ifname << " for " << mcast_addr << ":" << port;
    return base::ScopedFD();
  }
  struct ip_mreqn mreqn;
  memset(&mreqn, 0, sizeof(mreqn));
  mreqn.imr_multiaddr = mcast_addr;
  mreqn.imr_address.s_addr = htonl(INADDR_ANY);
  mreqn.imr_ifindex = ifindex;
  if (setsockopt(fd.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn,
                 sizeof(mreqn)) < 0) {
    PLOG(ERROR) << "Can't add multicast membership for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port;
    return base::ScopedFD();
  }

  int off = 0;
  if (setsockopt(fd.get(), IPPROTO_IP, IP_MULTICAST_LOOP, &off, sizeof(off))) {
    PLOG(ERROR)
        << "setsockopt(IP_MULTICAST_LOOP) failed for multicast forwarder on "
        << ifname << " for " << mcast_addr << ":" << port;
    return base::ScopedFD();
  }

  int on = 1;
  if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    PLOG(ERROR) << "setsockopt(SO_REUSEADDR) failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port;
    return base::ScopedFD();
  }

  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(port);

  if (bind(fd.get(), (const struct sockaddr*)&bind_addr, sizeof(bind_addr)) <
      0) {
    PLOG(ERROR) << "bind(" << port << ") failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port;
    return base::ScopedFD();
  }

  return fd;
}

bool MulticastForwarder::Start(const std::string& int_ifname,
                               const std::string& lan_ifname,
                               uint32_t mdns_ipaddr,
                               uint32_t mcast_addr,
                               uint16_t port) {
  int_ifname_ = int_ifname;
  lan_ifname_ = lan_ifname;
  mcast_addr_.s_addr = mcast_addr;
  mdns_ip_.s_addr = mdns_ipaddr;
  port_ = port;

  base::ScopedFD int_fd(Bind(int_ifname_, mcast_addr_, port_));
  if (!int_fd.is_valid()) {
    LOG(WARNING) << "Could not bind socket on " << int_ifname_ << " for "
                 << mcast_addr_ << ":" << port_;
    return false;
  }
  int_socket_.reset(new Socket(
      std::move(int_fd),
      base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                          base::Unretained(this))));

  base::ScopedFD lan_fd(Bind(lan_ifname_, mcast_addr_, port_));
  if (!lan_fd.is_valid()) {
    LOG(WARNING) << "Could not bind socket on " << lan_ifname_ << " for "
                 << mcast_addr_ << ":" << port_;
    int_socket_.reset();
    return false;
  }
  lan_socket_.reset(new Socket(
      std::move(lan_fd),
      base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                          base::Unretained(this))));

  LOG(INFO) << "Started forwarding between " << lan_ifname_ << " and "
            << int_ifname_ << " for " << mcast_addr_ << ":" << port_;

  return true;
}

// This callback is registered as part of MulticastSocket::Bind().
// All of our sockets use this function as a common callback.
void MulticastForwarder::OnFileCanReadWithoutBlocking(int fd) {
  char data[kBufSize];
  struct sockaddr_in fromaddr;
  socklen_t addrlen = sizeof(fromaddr);

  ssize_t len =
      recvfrom(fd, data, kBufSize, 0,
               reinterpret_cast<struct sockaddr*>(&fromaddr), &addrlen);
  if (len < 0) {
    PLOG(WARNING) << "recvfrom failed";
    return;
  }
  if (addrlen != sizeof(fromaddr)) {
    LOG(WARNING) << "recvfrom failed: unexpected src addr length " << addrlen;
    return;
  }

  uint16_t src_port = ntohs(fromaddr.sin_port);

  struct sockaddr_in dst = {0};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(port_);
  dst.sin_addr = mcast_addr_;

  // Forward ingress traffic to guest.
  if (fd == lan_socket_->fd.get()) {
    if (sendto(int_socket_->fd.get(), data, len, 0,
               reinterpret_cast<const struct sockaddr*>(&dst),
               sizeof(struct sockaddr_in)) < 0) {
      PLOG(WARNING) << "sendto failed";
    }
    return;
  }

  // Forward egress traffic from guest.
  if (fd != int_socket_->fd.get())
    return;

  // Forward mDNS egress multicast traffic from the guest to the physical
  // network. This requires translating any IPv4 address specific to the guest
  // and not visible to the physical network.
  if (port_ == kMdnsPort) {
    // TODO(b/132574450) The replacement address should instead be specified
    // as an input argument, based on the properties of the network currently
    // connected on |lan_ifname_|.
    const struct in_addr lan_ip =
        GetInterfaceIp(lan_socket_->fd.get(), lan_ifname_);
    if (lan_ip.s_addr == htonl(INADDR_ANY)) {
      // When the physical interface has no IPv4 address, IPv4 is not
      // provisioned and there is no point in trying to forward traffic in
      // either direction.
      return;
    }
    TranslateMdnsIp(lan_ip, mdns_ip_, data, len);
  }

  SendTo(src_port, data, len, dst);
}

bool MulticastForwarder::SendTo(uint16_t src_port,
                                const void* data,
                                ssize_t len,
                                const struct sockaddr_in& dst) {
  if (src_port == port_) {
    if (sendto(lan_socket_->fd.get(), data, len, 0,
               reinterpret_cast<const struct sockaddr*>(&dst),
               sizeof(struct sockaddr_in)) < 0) {
      PLOG(WARNING) << "sendto failed";
      return false;
    }
    return true;
  }

  arc_networkd::Socket temp_socket(AF_INET, SOCK_DGRAM);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, lan_ifname_.c_str(), IFNAMSIZ);
  if (setsockopt(temp_socket.fd(), SOL_SOCKET, SO_BINDTODEVICE, &ifr,
                 sizeof(ifr))) {
    PLOG(ERROR) << "setsockopt(SOL_SOCKET) failed";
    return false;
  }

  int off = 0;
  if (setsockopt(temp_socket.fd(), IPPROTO_IP, IP_MULTICAST_LOOP, &off,
                 sizeof(off))) {
    PLOG(ERROR) << "setsockopt(IP_MULTICAST_LOOP) failed";
    return false;
  }

  struct sockaddr_in bind_addr;
  memset(&bind_addr, 0, sizeof(bind_addr));
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(src_port);

  if (!temp_socket.Bind(reinterpret_cast<const struct sockaddr*>(&bind_addr),
                        sizeof(bind_addr)))
    return false;

  return temp_socket.SendTo(data, len,
                            reinterpret_cast<const struct sockaddr*>(&dst),
                            sizeof(struct sockaddr_in));
}

// static
void MulticastForwarder::TranslateMdnsIp(const struct in_addr& lan_ip,
                                         const struct in_addr& guest_ip,
                                         char* data,
                                         ssize_t len) {
  if (guest_ip.s_addr == htonl(INADDR_ANY)) {
    return;
  }

  // Make sure this is a valid, successful DNS response from the Android host.
  if (len > net::dns_protocol::kMaxUDPSize || len <= 0) {
    return;
  }

  net::DnsResponse resp;
  memcpy(resp.io_buffer()->data(), data, len);
  if (!resp.InitParseWithoutQuery(len) ||
      !(resp.flags() & net::dns_protocol::kFlagResponse) ||
      resp.rcode() != net::dns_protocol::kRcodeNOERROR) {
    return;
  }

  // Check all A records for the internal IP, and replace it with |lan_ip|
  // if it is found.
  net::DnsRecordParser parser = resp.Parser();
  while (!parser.AtEnd()) {
    const size_t ipv4_addr_len = sizeof(lan_ip.s_addr);

    net::DnsResourceRecord record;
    if (!parser.ReadRecord(&record)) {
      break;
    }
    if (record.type == net::dns_protocol::kTypeA &&
        record.rdata.size() == ipv4_addr_len) {
      const char* rr_ip = record.rdata.data();
      if (guest_ip.s_addr ==
          reinterpret_cast<const struct in_addr*>(rr_ip)->s_addr) {
        // HACK: This is able to calculate the (variable) offset of the IPv4
        // address inside the resource record by assuming that the StringPiece
        // returns a pointer inside the io_buffer.  It works today, but
        // future libchrome changes might break it.
        size_t ip_offset = rr_ip - resp.io_buffer()->data();
        CHECK(ip_offset <= len - ipv4_addr_len);
        memcpy(&data[ip_offset], &lan_ip.s_addr, ipv4_addr_len);
      }
    }
  }
}

}  // namespace arc_networkd

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
#include <sysexits.h>

#include <utility>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>

#include "arc/network/dns/dns_protocol.h"
#include "arc/network/dns/dns_response.h"
#include "arc/network/minijailed_process_runner.h"
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

// Fills sockaddr_storage values.
void SetSockaddr(struct sockaddr_storage* saddr_storage,
                 sa_family_t sa_family,
                 uint16_t port,
                 char* addr) {
  struct sockaddr* saddr = reinterpret_cast<sockaddr*>(saddr_storage);
  if (sa_family == AF_INET) {
    struct sockaddr_in* saddr4 = reinterpret_cast<struct sockaddr_in*>(saddr);
    saddr4->sin_family = AF_INET;
    saddr4->sin_port = htons(port);
    if (addr)
      memcpy(&saddr4->sin_addr, addr, sizeof(struct in_addr));
    return;
  }
  if (sa_family == AF_INET6) {
    struct sockaddr_in6* saddr6 = reinterpret_cast<sockaddr_in6*>(saddr);
    saddr6->sin6_family = AF_INET6;
    saddr6->sin6_port = htons(port);
    if (addr)
      memcpy(&saddr6->sin6_addr, addr, sizeof(struct in6_addr));
    return;
  }
  LOG(ERROR) << "Invalid socket family " << sa_family;
}

}  // namespace

namespace arc_networkd {

MulticastForwarder::Socket::Socket(
    base::ScopedFD fd,
    sa_family_t sa_family,
    const base::Callback<void(int, sa_family_t)>& callback)
    : fd(std::move(fd)) {
  watcher = base::FileDescriptorWatcher::WatchReadable(
      Socket::fd.get(),
      base::BindRepeating(callback, Socket::fd.get(), sa_family));
}

MulticastForwarder::MulticastForwarder(const std::string& lan_ifname,
                                       uint32_t mcast_addr,
                                       const std::string& mcast_addr6,
                                       uint16_t port)
    : lan_ifname_(lan_ifname), port_(port) {
  mcast_addr_.s_addr = mcast_addr;
  CHECK(inet_pton(AF_INET6, mcast_addr6.c_str(), mcast_addr6_.s6_addr));

  base::ScopedFD lan_fd(Bind(AF_INET, lan_ifname_));
  if (!lan_fd.is_valid()) {
    LOG(WARNING) << "Could not bind socket on " << lan_ifname_ << " for "
                 << mcast_addr_ << ":" << port_;
  }

  base::ScopedFD lan_fd6(Bind(AF_INET6, lan_ifname_));
  if (!lan_fd6.is_valid()) {
    LOG(WARNING) << "Could not bind socket on " << lan_ifname_ << " for "
                 << mcast_addr6_ << ":" << port_;
  }

  lan_socket_.emplace(
      AF_INET, new Socket(std::move(lan_fd), AF_INET,
                          base::BindRepeating(
                              &MulticastForwarder::OnFileCanReadWithoutBlocking,
                              base::Unretained(this))));

  lan_socket_.emplace(
      AF_INET6,
      new Socket(
          std::move(lan_fd6), AF_INET6,
          base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                              base::Unretained(this))));
}

base::ScopedFD MulticastForwarder::Bind(sa_family_t sa_family,
                                        const std::string& ifname) {
  char mcast_addr[INET6_ADDRSTRLEN];
  inet_ntop(sa_family,
            sa_family == AF_INET ? reinterpret_cast<const void*>(&mcast_addr_)
                                 : reinterpret_cast<const void*>(&mcast_addr6_),
            mcast_addr, INET6_ADDRSTRLEN);

  base::ScopedFD fd(socket(sa_family, SOCK_DGRAM, 0));
  if (!fd.is_valid()) {
    PLOG(ERROR) << "socket() failed for multicast forwarder on " << ifname
                << " for " << mcast_addr << ":" << port_;
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
                << ifname << " for " << mcast_addr << ":" << port_;
    return base::ScopedFD();
  }

  int ifindex = if_nametoindex(ifname.c_str());
  if (ifindex == 0) {
    PLOG(ERROR)
        << "Could not obtain interface index for multicast forwarder on "
        << ifname << " for " << mcast_addr << ":" << port_;
    return base::ScopedFD();
  }

  int level, optname;
  if (sa_family == AF_INET) {
    struct ip_mreqn mreqn;
    memset(&mreqn, 0, sizeof(mreqn));
    mreqn.imr_multiaddr = mcast_addr_;
    mreqn.imr_address.s_addr = htonl(INADDR_ANY);
    mreqn.imr_ifindex = ifindex;
    if (setsockopt(fd.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn,
                   sizeof(mreqn)) < 0) {
      PLOG(ERROR)
          << "Can't add multicast membership for multicast forwarder on "
          << ifname << " for " << mcast_addr_ << ":" << port_;
      return base::ScopedFD();
    }

    level = IPPROTO_IP;
    optname = IP_MULTICAST_LOOP;
  } else if (sa_family == AF_INET6) {
    struct ipv6_mreq mreqn;
    memset(&mreqn, 0, sizeof(mreqn));
    mreqn.ipv6mr_multiaddr = mcast_addr6_;
    mreqn.ipv6mr_interface = ifindex;
    if (setsockopt(fd.get(), IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreqn,
                   sizeof(mreqn)) < 0) {
      PLOG(ERROR)
          << "Can't add multicast membership for multicast forwarder on "
          << ifname << " for " << mcast_addr6_ << ":" << port_;
      return base::ScopedFD();
    }

    level = IPPROTO_IPV6;
    optname = IPV6_MULTICAST_LOOP;
  } else {
    return base::ScopedFD();
  }

  int off = 0;
  if (setsockopt(fd.get(), level, optname, &off, sizeof(off))) {
    PLOG(ERROR)
        << "setsockopt(IP_MULTICAST_LOOP) failed for multicast forwarder on "
        << ifname << " for " << mcast_addr << ":" << port_;
    return base::ScopedFD();
  }

  int on = 1;
  if (setsockopt(fd.get(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    PLOG(ERROR) << "setsockopt(SO_REUSEADDR) failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port_;
    return base::ScopedFD();
  }

  struct sockaddr_storage bind_addr = {0};
  SetSockaddr(&bind_addr, sa_family, port_, nullptr);

  if (bind(fd.get(), (const struct sockaddr*)&bind_addr,
           sizeof(struct sockaddr_storage)) < 0) {
    PLOG(ERROR) << "bind(" << port_ << ") failed for multicast forwarder on "
                << ifname << " for " << mcast_addr << ":" << port_;
    return base::ScopedFD();
  }

  return fd;
}

bool MulticastForwarder::AddGuest(const std::string& int_ifname,
                                  uint32_t guest_addr) {
  if (int_sockets_.find(std::make_pair(AF_INET, int_ifname)) !=
          int_sockets_.end() ||
      int_sockets_.find(std::make_pair(AF_INET6, int_ifname)) !=
          int_sockets_.end()) {
    LOG(WARNING) << "Forwarding is already started between " << lan_ifname_
                 << " and " << int_ifname;
    return false;
  }

  bool success = false;

  // Set up IPv4 multicast forwarder.
  base::ScopedFD int_fd4(Bind(AF_INET, int_ifname));
  if (int_fd4.is_valid()) {
    struct in_addr guest_ip4 = {0};
    guest_ip4.s_addr = guest_addr;
    int_ips_.emplace(std::make_pair(AF_INET, int_fd4.get()), guest_ip4);

    std::unique_ptr<Socket> int_socket4 = std::make_unique<Socket>(
        std::move(int_fd4), AF_INET,
        base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                            base::Unretained(this)));

    int_sockets_.emplace(std::make_pair(AF_INET, int_ifname),
                         std::move(int_socket4));

    success = true;
    LOG(INFO) << "Started IPv4 forwarding between " << lan_ifname_ << " and "
              << int_ifname << " for " << mcast_addr_ << ":" << port_;
  } else {
    LOG(WARNING) << "Could not bind socket on " << int_ifname << " for "
                 << mcast_addr_ << ":" << port_;
  }

  // Set up IPv6 multicast forwarder.
  base::ScopedFD int_fd6(Bind(AF_INET6, int_ifname));
  if (int_fd6.is_valid()) {
    int_ips_.emplace(std::make_pair(AF_INET6, int_fd6.get()), in_addr{});

    std::unique_ptr<Socket> int_socket6 = std::make_unique<Socket>(
        std::move(int_fd6), AF_INET6,
        base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                            base::Unretained(this)));

    int_sockets_.emplace(std::make_pair(AF_INET6, int_ifname),
                         std::move(int_socket6));

    success = true;
    LOG(INFO) << "Started IPv6 forwarding between " << lan_ifname_ << " and "
              << int_ifname << " for " << mcast_addr6_ << ":" << port_;
  } else {
    LOG(WARNING) << "Could not bind socket on " << int_ifname << " for "
                 << mcast_addr6_ << ":" << port_;
  }

  return success;
}

void MulticastForwarder::RemoveGuest(const std::string& int_ifname) {
  const auto& socket4 = int_sockets_.find(std::make_pair(AF_INET, int_ifname));
  if (socket4 != int_sockets_.end()) {
    int_ips_.erase(std::make_pair(AF_INET, socket4->second->fd.get()));
    int_sockets_.erase(socket4);
  } else {
    LOG(WARNING) << "IPv4 forwarding is not started between " << lan_ifname_
                 << " and " << int_ifname;
  }

  const auto& socket6 = int_sockets_.find(std::make_pair(AF_INET6, int_ifname));
  if (socket6 != int_sockets_.end()) {
    int_ips_.erase(std::make_pair(AF_INET6, socket6->second->fd.get()));
    int_sockets_.erase(socket6);
  } else {
    LOG(WARNING) << "IPv6 forwarding is not started between " << lan_ifname_
                 << " and " << int_ifname;
  }
}

void MulticastForwarder::OnFileCanReadWithoutBlocking(int fd,
                                                      sa_family_t sa_family) {
  DCHECK(sa_family != AF_INET && sa_family != AF_INET6);

  char data[kBufSize];

  struct sockaddr_storage fromaddr_storage = {0};
  struct sockaddr* fromaddr =
      reinterpret_cast<struct sockaddr*>(&fromaddr_storage);

  socklen_t addrlen = sizeof(struct sockaddr_storage);

  ssize_t len = recvfrom(fd, data, kBufSize, 0, fromaddr, &addrlen);
  if (len < 0) {
    PLOG(WARNING) << "recvfrom failed";
    return;
  }

  socklen_t expectlen = sa_family == AF_INET ? sizeof(struct sockaddr_in)
                                             : sizeof(struct sockaddr_in6);
  if (addrlen != expectlen) {
    LOG(WARNING) << "recvfrom failed: unexpected src addr length " << addrlen;
    return;
  }

  struct sockaddr_storage dst_storage = {0};
  struct sockaddr* dst = reinterpret_cast<struct sockaddr*>(&dst_storage);
  uint16_t src_port;

  if (sa_family == AF_INET) {
    const struct sockaddr_in* addr4 =
        reinterpret_cast<const struct sockaddr_in*>(fromaddr);
    src_port = ntohs(addr4->sin_port);
  } else if (sa_family == AF_INET6) {
    const struct sockaddr_in6* addr6 =
        reinterpret_cast<const struct sockaddr_in6*>(fromaddr);
    src_port = ntohs(addr6->sin6_port);
  }
  SetSockaddr(&dst_storage, sa_family, port_,
              sa_family == AF_INET ? reinterpret_cast<char*>(&mcast_addr_)
                                   : reinterpret_cast<char*>(&mcast_addr6_));

  // Forward ingress traffic to all guests.
  const auto& lan_socket = lan_socket_.find(sa_family);
  if ((lan_socket != lan_socket_.end() && fd == lan_socket->second->fd.get())) {
    SendToGuests(data, len, dst, addrlen);
    return;
  }

  const auto& int_ip = int_ips_.find(std::make_pair(sa_family, fd));
  if (int_ip == int_ips_.end() || lan_socket == lan_socket_.end())
    return;

  // Forward egress traffic from one guest to all other guests.
  // No IP translation is required as other guests can route to each other
  // behind the SNAT setup.
  SendToGuests(data, len, dst, addrlen, fd);

  // On mDNS, sending to physical network requires translating any IPv4
  // address specific to the guest and not visible to the physical network.
  if (sa_family == AF_INET && port_ == kMdnsPort) {
    // TODO(b/132574450) The replacement address should instead be specified
    // as an input argument, based on the properties of the network
    // currently connected on |lan_ifname_|.
    const struct in_addr lan_ip =
        GetInterfaceIp(lan_socket->second->fd.get(), lan_ifname_);
    if (lan_ip.s_addr == htonl(INADDR_ANY)) {
      // When the physical interface has no IPv4 address, IPv4 is not
      // provisioned and there is no point in trying to forward traffic in
      // either direction.
      return;
    }
    TranslateMdnsIp(lan_ip, int_ip->second, data, len);
  }

  // Forward egress traffic from one guest to outside network.
  SendTo(src_port, data, len, dst, addrlen);
}

bool MulticastForwarder::SendTo(uint16_t src_port,
                                const void* data,
                                ssize_t len,
                                const struct sockaddr* dst,
                                socklen_t dst_len) {
  if (src_port == port_) {
    int lan_fd = lan_socket_.find(dst->sa_family)->second->fd.get();
    if (sendto(lan_fd, data, len, 0, dst, dst_len) < 0) {
      PLOG(WARNING) << "sendto failed";
      return false;
    }
    return true;
  }

  arc_networkd::Socket temp_socket(dst->sa_family, SOCK_DGRAM);

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, lan_ifname_.c_str(), IFNAMSIZ);
  if (setsockopt(temp_socket.fd(), SOL_SOCKET, SO_BINDTODEVICE, &ifr,
                 sizeof(ifr))) {
    PLOG(ERROR) << "setsockopt(SOL_SOCKET) failed";
    return false;
  }

  int level, optname;
  struct sockaddr_storage bind_addr_storage = {0};
  struct sockaddr* bind_addr = reinterpret_cast<sockaddr*>(&bind_addr_storage);
  if (dst->sa_family == AF_INET) {
    level = IPPROTO_IP;
    optname = IP_MULTICAST_LOOP;
  } else if (dst->sa_family == AF_INET6) {
    level = IPPROTO_IPV6;
    optname = IPV6_MULTICAST_LOOP;
  } else {
    return false;
  }
  SetSockaddr(&bind_addr_storage, dst->sa_family, src_port, nullptr);

  int off = 0;
  if (setsockopt(temp_socket.fd(), level, optname, &off, sizeof(off))) {
    PLOG(ERROR) << "setsockopt(IP_MULTICAST_LOOP) failed";
    return false;
  }

  if (!temp_socket.Bind(bind_addr, sizeof(struct sockaddr_storage)))
    return false;

  return temp_socket.SendTo(data, len, dst, dst_len);
}

bool MulticastForwarder::SendToGuests(const void* data,
                                      ssize_t len,
                                      const struct sockaddr* dst,
                                      socklen_t dst_len,
                                      int ignore_fd) {
  bool success = true;
  for (const auto& socket : int_sockets_) {
    if (socket.first.first != dst->sa_family)
      continue;
    int fd = socket.second->fd.get();
    if (fd == ignore_fd)
      continue;

    // Use already created multicast fd.
    if (sendto(fd, data, len, 0, dst, dst_len) < 0) {
      PLOG(WARNING) << "sendto failed to " << socket.first.second;
      success = false;
    }
  }
  return success;
}

// static
void MulticastForwarder::TranslateMdnsIp(const struct in_addr& lan_ip,
                                         const struct in_addr& guest_ip,
                                         char* data,
                                         ssize_t len) {
  if (guest_ip.s_addr == htonl(INADDR_ANY)) {
    return;
  }

  // Make sure this is a valid, successful DNS response from the Android
  // host.
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
      struct in_addr rr_ip;
      memcpy(&rr_ip, record.rdata.data(), ipv4_addr_len);
      if (guest_ip.s_addr == rr_ip.s_addr) {
        // HACK: This is able to calculate the (variable) offset of the IPv4
        // address inside the resource record by assuming that the
        // StringPiece returns a pointer inside the io_buffer.  It works
        // today, but future libchrome changes might break it.
        size_t ip_offset = record.rdata.data() - resp.io_buffer()->data();
        CHECK(ip_offset <= len - ipv4_addr_len);
        memcpy(&data[ip_offset], &lan_ip.s_addr, ipv4_addr_len);
      }
    }
  }
}

MulticastProxy::MulticastProxy(base::ScopedFD control_fd)
    : msg_dispatcher_(std::move(control_fd)) {
  msg_dispatcher_.RegisterFailureHandler(base::Bind(
      &MulticastProxy::OnParentProcessExit, weak_factory_.GetWeakPtr()));

  msg_dispatcher_.RegisterDeviceMessageHandler(
      base::Bind(&MulticastProxy::OnDeviceMessage, weak_factory_.GetWeakPtr()));
}

int MulticastProxy::OnInit() {
  // Prevent the main process from sending us any signals.
  if (setsid() < 0) {
    PLOG(ERROR) << "Failed to created a new session with setsid; exiting";
    return EX_OSERR;
  }

  EnterChildProcessJail();
  return Daemon::OnInit();
}

void MulticastProxy::Reset() {
  mdns_fwds_.clear();
  ssdp_fwds_.clear();
}

void MulticastProxy::OnParentProcessExit() {
  LOG(ERROR) << "Quitting because the parent process died";
  Reset();
  Quit();
}

void MulticastProxy::OnDeviceMessage(const DeviceMessage& msg) {
  const std::string& dev_ifname = msg.dev_ifname();
  if (dev_ifname.empty()) {
    LOG(DFATAL) << "Received DeviceMessage w/ empty dev_ifname";
    return;
  }
  uint32_t guest_ip = msg.guest_ip4addr();

  auto mdns_fwd = mdns_fwds_.find(dev_ifname);
  auto ssdp_fwd = ssdp_fwds_.find(dev_ifname);

  if (!msg.has_teardown()) {
    // Start multicast forwarders.
    if (mdns_fwd == mdns_fwds_.end()) {
      LOG(INFO) << "Enabling mDNS forwarding for device " << dev_ifname;
      auto fwd = std::make_unique<MulticastForwarder>(
          dev_ifname, kMdnsMcastAddress, kMdnsMcastAddress6, kMdnsPort);
      mdns_fwd = mdns_fwds_.emplace(dev_ifname, std::move(fwd)).first;
    }

    LOG(INFO) << "Starting mDNS forwarding between " << dev_ifname << " and "
              << msg.br_ifname();
    if (!mdns_fwd->second->AddGuest(msg.br_ifname(), guest_ip)) {
      LOG(WARNING) << "mDNS forwarder could not be started on " << dev_ifname;
    }

    if (ssdp_fwd == ssdp_fwds_.end()) {
      LOG(INFO) << "Enabling SSDP forwarding for device " << dev_ifname;
      auto fwd = std::make_unique<MulticastForwarder>(
          dev_ifname, kSsdpMcastAddress, kSsdpMcastAddress6, kSsdpPort);
      ssdp_fwd = ssdp_fwds_.emplace(dev_ifname, std::move(fwd)).first;
    }

    LOG(INFO) << "Starting SSDP forwarding between " << dev_ifname << " and "
              << msg.br_ifname();
    if (!ssdp_fwd->second->AddGuest(msg.br_ifname(), htonl(INADDR_ANY))) {
      LOG(WARNING) << "SSDP forwarder could not be started on " << dev_ifname;
    }

    return;
  }

  if (msg.has_br_ifname()) {
    // A bridge interface is removed.
    if (mdns_fwd != mdns_fwds_.end()) {
      LOG(INFO) << "Disabling mDNS forwarding between " << dev_ifname << " and "
                << msg.br_ifname();
      mdns_fwd->second->RemoveGuest(msg.br_ifname());
    }
    if (ssdp_fwd != ssdp_fwds_.end()) {
      LOG(INFO) << "Disabling SSDP forwarding between " << dev_ifname << " and "
                << msg.br_ifname();
      ssdp_fwd->second->RemoveGuest(msg.br_ifname());
    }
    return;
  }

  // A physical interface is removed.
  if (mdns_fwd != mdns_fwds_.end()) {
    LOG(INFO) << "Disabling mDNS forwarding for physical interface "
              << dev_ifname;
    mdns_fwds_.erase(mdns_fwd);
  }
  if (ssdp_fwd != ssdp_fwds_.end()) {
    LOG(INFO) << "Disabling SSDP forwarding for physical interface "
              << dev_ifname;
    ssdp_fwds_.erase(ssdp_fwd);
  }
}

}  // namespace arc_networkd

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
}  // namespace

namespace arc_networkd {

MulticastForwarder::Socket::Socket(base::ScopedFD fd,
                                   const base::Callback<void(int)>& callback)
    : fd(std::move(fd)) {
  watcher = base::FileDescriptorWatcher::WatchReadable(
      Socket::fd.get(), base::BindRepeating(callback, Socket::fd.get()));
}

MulticastForwarder::MulticastForwarder(const std::string& lan_ifname,
                                       uint32_t mcast_addr,
                                       uint16_t port)
    : lan_ifname_(lan_ifname), port_(port) {
  mcast_addr_.s_addr = mcast_addr;
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
    PLOG(ERROR) << "setsockopt(IP_MULTICAST_LOOP) failed for multicast "
                   "forwarder on "
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

bool MulticastForwarder::AddGuest(const std::string& int_ifname,
                                  uint32_t guest_addr) {
  base::ScopedFD int_fd(Bind(int_ifname, mcast_addr_, port_));
  if (!int_fd.is_valid()) {
    LOG(WARNING) << "Could not bind socket on " << int_ifname << " for "
                 << mcast_addr_ << ":" << port_;
    return false;
  }

  struct in_addr guest_ip = {0};
  guest_ip.s_addr = guest_addr;
  int_ips_.emplace(int_fd.get(), guest_ip);

  std::unique_ptr<Socket> int_socket = std::make_unique<Socket>(
      std::move(int_fd),
      base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                          base::Unretained(this)));

  int_sockets_.emplace(int_ifname, std::move(int_socket));

  // Multicast forwarder is not started yet.
  if (lan_socket_ == nullptr) {
    base::ScopedFD lan_fd(Bind(lan_ifname_, mcast_addr_, port_));
    if (!lan_fd.is_valid()) {
      LOG(WARNING) << "Could not bind socket on " << lan_ifname_ << " for "
                   << mcast_addr_ << ":" << port_;
      int_ips_.clear();
      int_sockets_.clear();
      return false;
    }
    lan_socket_.reset(new Socket(
        std::move(lan_fd),
        base::BindRepeating(&MulticastForwarder::OnFileCanReadWithoutBlocking,
                            base::Unretained(this))));
  }

  LOG(INFO) << "Started forwarding between " << lan_ifname_ << " and "
            << int_ifname << " for " << mcast_addr_ << ":" << port_;

  return true;
}

void MulticastForwarder::RemoveGuest(const std::string& int_ifname) {
  const auto& socket = int_sockets_.find(int_ifname);
  if (socket == int_sockets_.end()) {
    LOG(WARNING) << "Failed to remove guest interface " << int_ifname;
    return;
  }

  int_ips_.erase(socket->second->fd.get());
  int_sockets_.erase(socket);
}

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

  // Forward ingress traffic to all guests.
  if (fd == lan_socket_->fd.get()) {
    SendToGuests(data, len, dst);
    return;
  }

  const auto& int_ip = int_ips_.find(fd);
  if (int_ip == int_ips_.end())
    return;

  // Forward egress traffic from one guest to all other guests.
  // No IP translation is required as other guests can route to each other
  // behind the SNAT setup.
  SendToGuests(data, len, dst, fd);

  // On mDNS, sending to physical network requires translating any IPv4
  // address specific to the guest and not visible to the physical network.
  if (port_ == kMdnsPort) {
    // TODO(b/132574450) The replacement address should instead be specified
    // as an input argument, based on the properties of the network
    // currently connected on |lan_ifname_|.
    const struct in_addr lan_ip =
        GetInterfaceIp(lan_socket_->fd.get(), lan_ifname_);
    if (lan_ip.s_addr == htonl(INADDR_ANY)) {
      // When the physical interface has no IPv4 address, IPv4 is not
      // provisioned and there is no point in trying to forward traffic in
      // either direction.
      return;
    }
    TranslateMdnsIp(lan_ip, int_ip->second, data, len);
  }

  // Forward egress traffic from one guest to outside network.
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

bool MulticastForwarder::SendToGuests(const void* data,
                                      ssize_t len,
                                      const struct sockaddr_in& dst,
                                      int ignore_fd) {
  bool success = true;
  for (const auto& socket : int_sockets_) {
    int fd = socket.second->fd.get();
    if (fd == ignore_fd)
      continue;

    // Use already created multicast fd.
    if (sendto(fd, data, len, 0, reinterpret_cast<const struct sockaddr*>(&dst),
               sizeof(struct sockaddr_in)) < 0) {
      PLOG(WARNING) << "sendto failed to " << socket.first;
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
      const char* rr_ip = record.rdata.data();
      if (guest_ip.s_addr ==
          reinterpret_cast<const struct in_addr*>(rr_ip)->s_addr) {
        // HACK: This is able to calculate the (variable) offset of the IPv4
        // address inside the resource record by assuming that the
        // StringPiece returns a pointer inside the io_buffer.  It works
        // today, but future libchrome changes might break it.
        size_t ip_offset = rr_ip - resp.io_buffer()->data();
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
    return -1;
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
          dev_ifname, kMdnsMcastAddress, kMdnsPort);
      mdns_fwd = mdns_fwds_.emplace(dev_ifname, std::move(fwd)).first;
    }

    LOG(INFO) << "Starting mDNS forwarding for guest interface "
              << msg.br_ifname();
    if (!mdns_fwd->second->AddGuest(msg.br_ifname(), guest_ip)) {
      LOG(WARNING) << "mDNS forwarder could not be started on " << dev_ifname;
    }

    if (ssdp_fwd == ssdp_fwds_.end()) {
      LOG(INFO) << "Enabling SSDP forwarding for device " << dev_ifname;
      auto fwd = std::make_unique<MulticastForwarder>(
          dev_ifname, kSsdpMcastAddress, kSsdpPort);
      ssdp_fwd = ssdp_fwds_.emplace(dev_ifname, std::move(fwd)).first;
    }

    LOG(INFO) << "Starting SSDP forwarding for guest interface "
              << msg.br_ifname();
    if (!ssdp_fwd->second->AddGuest(msg.br_ifname(), htonl(INADDR_ANY))) {
      LOG(WARNING) << "SSDP forwarder could not be started on " << dev_ifname;
    }

    return;
  }

  if (msg.has_br_ifname()) {
    // A bridge interface is removed.
    if (mdns_fwd != mdns_fwds_.end()) {
      LOG(INFO) << "Disabling mDNS forwarding for guest interface "
                << msg.br_ifname();
      mdns_fwd->second->RemoveGuest(msg.br_ifname());
    }
    if (ssdp_fwd != ssdp_fwds_.end()) {
      LOG(INFO) << "Disabling SSDP forwarding for guest interface "
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

// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MULTICAST_FORWARDER_H_
#define ARC_NETWORK_MULTICAST_FORWARDER_H_

#include <netinet/ip.h>
#include <sys/socket.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/scoped_file.h>
#include <base/macros.h>
#include <brillo/daemons/daemon.h>

#include "arc/network/broadcast_forwarder.h"
#include "arc/network/message_dispatcher.h"
#include "arc/network/net_util.h"

namespace arc_networkd {

constexpr uint32_t kMdnsMcastAddress = Ipv4Addr(224, 0, 0, 251);
constexpr char kMdnsMcastAddress6[] = "ff02::fb";
constexpr uint16_t kMdnsPort = 5353;
constexpr uint32_t kSsdpMcastAddress = Ipv4Addr(239, 255, 255, 250);
constexpr char kSsdpMcastAddress6[] = "ff02::c";
constexpr uint16_t kSsdpPort = 1900;

// Listens on a well-known port and forwards multicast messages between
// network interfaces.  Handles mDNS, legacy mDNS, and SSDP messages.
// MulticastForwarder forwards multicast between 1 physical interface and
// many guest interfaces.
class MulticastForwarder {
 public:
  MulticastForwarder(const std::string& lan_ifname,
                     uint32_t mcast_addr,
                     const std::string& mcast_addr6,
                     uint16_t port);
  virtual ~MulticastForwarder() = default;

  // Start forwarding multicast packets between the guest's interface
  // |int_ifname| and the external LAN interface |lan_ifname|.  This
  // only forwards traffic on multicast address |mcast_addr_| or
  // |mcast_addr6_| and UDP port |port|.
  //
  // On IPv4, |guest_addr|, if != INADDR_ANY, will be used to rewrite
  // mDNS A records to use the IP address from |lan_ifname|.
  bool AddGuest(const std::string& int_ifname, uint32_t guest_addr);

  // Stop forwarding multicast packets between |int_ifname| and
  // |lan_ifname_|.
  void RemoveGuest(const std::string& int_ifname);

  // Rewrite mDNS A records pointing to |guest_ip| so that they point to
  // the IPv4 |lan_ip| assigned to physical interface instead, so that Android
  // can advertise services to devices on the LAN.  This modifies |data|, an
  // incoming packet that is |len| long.
  static void TranslateMdnsIp(const struct in_addr& lan_ip,
                              const struct in_addr& guest_ip,
                              char* data,
                              ssize_t len);

 protected:
  // Socket is used to keep track of an fd and its watcher.
  struct Socket {
    Socket(base::ScopedFD fd,
           sa_family_t sa_family,
           const base::Callback<void(int, sa_family_t)>& callback);
    base::ScopedFD fd;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher;
  };

  // Bind will create a multicast socket and return its fd.
  base::ScopedFD Bind(sa_family_t sa_family, const std::string& ifname);

  // SendTo sends |data| using a socket bound to |src_port| and |lan_ifname_|.
  // If |src_port| is equal to |port_|, we will use |lan_socket_|. Otherwise,
  // create a temporary socket.
  bool SendTo(uint16_t src_port,
              const void* data,
              ssize_t len,
              const struct sockaddr* dst,
              socklen_t dst_len);

  // SendToGuests will forward packet to all Chrome OS guests' (ARC++,
  // Crostini, etc) internal fd using |port|.
  // However, if ignore_fd is not 0, it will skip guest with fd = ignore_fd.
  bool SendToGuests(const void* data,
                    ssize_t len,
                    const struct sockaddr* dst,
                    socklen_t dst_len,
                    int ignore_fd = -1);

  std::string lan_ifname_;
  unsigned int port_;

  struct in_addr mcast_addr_;
  struct in6_addr mcast_addr6_;

  std::map<sa_family_t, std::unique_ptr<Socket>> lan_socket_;

  // Mapping from internal interface name to internal sockets.
  std::map<std::pair<sa_family_t, std::string>, std::unique_ptr<Socket>>
      int_sockets_;

  // A map of internal file descriptors (guest facing sockets) to its guest
  // IP address.
  // We don't care about guest IP address on IPv6 as we are not translating
  // anything, so in_addr for |int_ipvs6_| will always be empty.
  std::map<std::pair<sa_family_t, int>, struct in_addr> int_ips_;

 private:
  void OnFileCanReadWithoutBlocking(int fd, sa_family_t sa_family);

  DISALLOW_COPY_AND_ASSIGN(MulticastForwarder);
};

// MulticastProxy manages multiple MulticastForwarder instances to forward
// multicast for multiple physical interfaces.
class MulticastProxy : public brillo::Daemon {
 public:
  explicit MulticastProxy(base::ScopedFD control_fd);
  virtual ~MulticastProxy() = default;

 protected:
  int OnInit() override;

  void OnParentProcessExit();
  void OnDeviceMessage(const DeviceMessage& msg);

 private:
  void Reset();

  MessageDispatcher msg_dispatcher_;
  std::map<std::string, std::unique_ptr<MulticastForwarder>> mdns_fwds_;
  std::map<std::string, std::unique_ptr<MulticastForwarder>> ssdp_fwds_;
  std::map<std::string, std::unique_ptr<BroadcastForwarder>> bcast_fwds_;

  base::WeakPtrFactory<MulticastProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MulticastProxy);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MULTICAST_FORWARDER_H_

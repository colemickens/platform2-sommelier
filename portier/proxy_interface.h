// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_PROXY_INTERFACE_H_
#define PORTIER_PROXY_INTERFACE_H_

#include <memory>
#include <string>
#include <vector>

#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

#include "portier/ether_socket.h"
#include "portier/group.h"
#include "portier/group_manager.h"
#include "portier/icmpv6_socket.h"
#include "portier/interface_disable_labels.h"
#include "portier/ll_address.h"
#include "portier/nd_msg.h"
#include "portier/status.h"

namespace portier {

// Proxy interface wraps several sockets which configure and control
// a network interfaces.  The interface is used for sending and
// receiving IPv6 packets to be proxied between other proxy interfaces,
//
// The underlying interface must be communicating on a multicast capable
// link-layer network.  The link-layer must meet all functional
// requirements specied in RFC8200 for IPv6 networks.  Most Ethernet and
// virtual Ethernet networks most likely meet the requirements.
//
// The loopback interfaces cannot be used as it could result in packet
// flooding on ND Proxy nodes.
//
// The ICMPv6 socket is expected to only be used for sending messages
// and not for receiving.
//
// The initialization of this interface requires the process have
// CAP_NET_RAW.
class ProxyInterface : public GroupMemberInterface<ProxyInterface>,
                       public InterfaceDisableLabels {
 public:
  enum class State {
    kInvalid,
    kUninitialized,
    kProxyEnabled,
    kProxyDisabled,
    kDeinitialized
  };
  static std::string GetStateName(State state);

  // Creates a new proxy interface for the specified interface name
  // |if_name|.  The provided interface cannot be the loopback
  // interface.
  static std::unique_ptr<ProxyInterface> Create(const std::string& if_name);

  ~ProxyInterface();

  State state() const { return state_; }

  bool IsValid() const;

  // Interface getters.

  // Returns the network interface index, as used by the kernel.
  // Returns -1 if the interface is not initialized.
  int GetInterfaceIndex() const;

  // Returns the network interface name.
  const std::string& name() const;

  // Getters for socket file descriptors.  Returns -1 if the sockets
  // are not initialized.
  int GetNDFd() const;
  int GetIPv6Fd() const;
  int GetICMPFd() const;

  // L2 Info
  // Gets the unicast link-layer address of the interface.
  const LLAddress& ll_address() const { return ll_address_; }
  // Gets the link-layer MTU.
  uint32_t mtu() const { return mtu_; }

  // L3 information
  // Returns a list of all IPv6 addresses associated to this interface.
  std::vector<shill::IPAddress> GetIPv6AddressList() const {
    return ip_addresses_;
  }

  // Makes a system call to obtain all the IPv6 addresses assigned to
  // the interface and stores them internally.  Returns true if system
  // call succeeded and the IP list was updated, false otherwise.
  bool RefreshIPv6AddressList();

  // Checks if the provided IPv6 address is a one of the assigned
  // IPv6 address to this interface.
  bool HasIPv6Address(const shill::IPAddress& address) const;

  // Proxy State.
  // Returns true if the interface is in an initialized state.
  bool IsInitialized() const;
  // Returns true if the interface is currently enabled and should
  // be able to send and receive messages.
  bool IsEnabled() const;
  // Sets the interface into an enabled or disabled proxy state.  Only
  // possible if the interface is already in an initialized state,
  // otherwise the call will fail.
  bool EnableProxy();
  bool DisableProxy();
  // Cleans up the interface and all of its internal sockets.  After the
  // call the interface cannot be used.
  bool Deinitialize();

  // ND Proxy Methods.

  // Used to send an ND message via a proxy interface.  The provided ND
  // message will be copied and modified as required by the ND Proxy
  // protocol.
  // Note: This method is intended to send Neighbor Discovery messages
  // which were received on another interface and not locally generated.
  //
  // Args:
  // - |header_fields| Contains all of the header fields of the packet
  //   that were received on another interface.  Modifications to the
  //   header will be made as required.  It is expected that the IP
  //   addresses are IPv6.
  // - |destination_ll_address| The destination link-layer address as
  //   it appears in the neighbor cache.
  // - |nd_message| The original ND message that was received.  The message
  //   will be modified to contain the appropriate link-layer information
  //   of the outgoing interface.
  // Note: Both arguments are passed by value to allow modification to
  // the message and provide the option to move the contents.
  Status ProxyNeighborDiscoveryMessage(IPv6EtherHeader header_fields,
                                       const LLAddress& destination_ll_address,
                                       NeighborDiscoveryMessage nd_message);

  // Receives all IPv6 Neighbor Discovery Messages which contain
  // link-layer information in its payload.
  Status ReceiveNeighborDiscoveryMessage(IPv6EtherHeader* header_fields,
                                         NeighborDiscoveryMessage* nd_message);

  // Used for flush input buffer of waiting packets.  Used for when a
  // proxy is disabled but the file descriptor is still opened.
  Status DiscardNeighborDiscoveryInput();

  // Non-ND IPv6 Methods.

  // Similar to sending ND Messsages, this method should be used to
  // proxy all other types of IPv6 packets that are received on a
  // proxy interface.
  // Args:
  // - |header_fields| Contains all of the header fields of the packet
  //   that were received on another interface.  Modifications to the
  //   header will be made as required.  It is expected that the IP
  //   addresses are IPv6.
  // - |destination_ll_address| The destination link-layer address as it
  //   appears in the neighbor cache.
  // - |payload| The bytes of the upper-level contents in an IPv6
  //   packet.
  Status SendIPv6Packet(IPv6EtherHeader header_fields,
                        const LLAddress& destination_ll_address,
                        const shill::ByteString& payload);

  // Receive all IPv6 packets except Neighbor Discovery messages which
  // contain link-layer information.
  Status ReceiveIPv6Packet(IPv6EtherHeader* header_fields,
                           shill::ByteString* payload);

  Status DiscardIPv6Input();

  // ICMP Methods.

  // A delegation method for ICMPv6Socket::SendPacketTooBigMessage().
  Status SendPacketTooBigMessage(const shill::IPAddress& destination_address,
                                 uint32_t mtu,
                                 const IPv6EtherHeader& original_header,
                                 const shill::ByteString& original_body);

 protected:
  // Callback hooks from GroupMemberInterface.
  // Mark the interface as either being in a group or not.
  void PostJoinGroup() override { ClearGroupless(); }
  void PostLeaveGroup() override { MarkGroupless(); }

  // Callback hook from InterfaceDisableLabels.
  void OnEnabled() override { EnableProxy(); }
  void OnDisabled() override { DisableProxy(); }

 private:
  // Constructor.
  explicit ProxyInterface(const std::string& if_name);

  // Initializes the network interface and opens all the required
  // sockets for handling raw ether packets and ICMPv6 packets.  The
  // call can fail if the specified |if_name| does not identify a
  // non-loopback, IPv6 enabled, ethernet network interface.  Can also
  // fail if the process does not have CAP_NET_RAW capabilities.
  Status Init();

  // Used to clean up the object in the event of an error during
  // initialization.  Should only be called from Init().
  void MarkInvalid();

  // Closes any owned file descriptor that is currently opened.
  void CloseOpenedFds();

  // Refreshes the internal IPv6 address list, but does not check if the
  // interface is fully initialized like the public method.
  bool InternalRefreshIPv6AddressList();

  // Proxy state.  Valid active states are Enabled and Disabled, inactive
  // states include Unitialized (not yet called Init()) and
  // Deinitialized (similar to unitialized but implies it was previously
  // in some active state).  Invalid state implies there was an error while
  // initializing, or the provided interface name does not exist.
  State state_;

  // Interface name (Ex eth0).
  std::string name_;

  // L2 address of the interface, used for conversion of source link-layer
  // of certain proxied ND messages.
  LLAddress ll_address_;
  // Link MTU.
  uint32_t mtu_;

  // List of all IPv6 addresses assigned to this interface.
  std::vector<shill::IPAddress> ip_addresses_;

  // Neighbor Discovery socket, used for sending and receiving.
  std::unique_ptr<EtherSocket> nd_sock_;
  // Proxy FD, used for sending and receiving.
  std::unique_ptr<EtherSocket> ipv6_sock_;
  // ICMP socket, used for sending only.
  std::unique_ptr<ICMPv6Socket> icmp_sock_;

  friend class Group<ProxyInterface>;

  DISALLOW_COPY_AND_ASSIGN(ProxyInterface);
};

// A type alias for Groups of Proxy Interfaces.
using ProxyGroup = Group<ProxyInterface>;

// A type alias for the manager of Proxy Groups.
using ProxyGroupManager = GroupManager<ProxyInterface>;

}  // namespace portier

#endif  // PORTIER_PROXY_INTERFACE_H_

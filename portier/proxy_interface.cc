// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/proxy_interface.h"

#include <errno.h>
#include <ifaddrs.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>

#include <algorithm>
#include <utility>

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <base/stl_util.h>
#include <base/posix/safe_strerror.h>

#include "portier/ipv6_util.h"
#include "portier/nd_bpf.h"

namespace portier {

using std::string;
using std::unique_ptr;
using std::vector;

using shill::ByteString;
using shill::IPAddress;

using Code = Status::Code;
using State = ProxyInterface::State;

namespace {
// Required hop-limit of all outgoing ND Proxy packets.
constexpr uint8_t kProxiedHopLimit = 255;
}  // namespace

// static.
string ProxyInterface::GetStateName(State state) {
  switch (state) {
    case State::kInvalid:
      return "Invalid";
    case State::kUninitialized:
      return "Uninitialized";
    case State::kProxyEnabled:
      return "Enabled";
    case State::kProxyDisabled:
      return "Disabled";
    case State::kDeinitialized:
      return "Deinitialized";
  }
  return base::StringPrintf("Unknown (%d)", static_cast<int>(state));
}

// static.
unique_ptr<ProxyInterface> ProxyInterface::Create(const string& if_name) {
  unique_ptr<ProxyInterface> proxy_if(new ProxyInterface(if_name));
  const Status init_status = proxy_if->Init();
  if (!init_status) {
    proxy_if.reset();
  }

  return proxy_if;
}

// private.
ProxyInterface::ProxyInterface(const std::string& if_name)
    : state_(State::kUninitialized), name_(if_name), mtu_(0) {}

ProxyInterface::~ProxyInterface() {
  if (State::kInvalid == state_ || State::kUninitialized == state_ ||
      State::kDeinitialized == state_) {
    // Nothing to do.
    return;
  }

  state_ = State::kDeinitialized;
}

// private.
Status ProxyInterface::Init() {
  DCHECK_NE(state(), State::kInvalid);

  if (name().empty()) {
    MarkInvalid();
    return Status(Code::INVALID_ARGUMENT,
                  "Empty string is not a valid interface name");
  }

  // Open ND ethernet socket.
  auto nd_sock = EtherSocket::Create(name());
  if (!nd_sock) {
    MarkInvalid();
    return Status(Code::UNEXPECTED_FAILURE,
                  "Failed to initialize the ND ether socket");
  }
  nd_sock_ = std::move(nd_sock);

  Status status = nd_sock_->SetNonBlockingMode(true);
  if (!status) {
    MarkInvalid();
    return status;
  }

  // Check that the interface is not the loopback interface.  Using
  // loopback interface as a proxy interface will cause echoed and/or
  // duplicate multicast packet proxying.
  bool loopback_flag = false;
  status = nd_sock_->GetLoopbackFlag(&loopback_flag);
  if (!status) {
    MarkInvalid();
    return status;
  }
  if (loopback_flag) {
    MarkInvalid();
    return Status(Code::INVALID_ARGUMENT)
           << "Cannot make a loopback interface (" << name()
           << ") into a proxy interface";
  }

  status = nd_sock_->SetAllMulticastMode(true);
  if (!status) {
    MarkInvalid();
    return status;
  }

  status = nd_sock_->AttachFilter(&kNeighborDiscoveryFilter);
  // Attacted filter.
  if (!status) {
    MarkInvalid();
    return status;
  }

  // Open IPv6 socket.
  auto ipv6_sock = EtherSocket::Create(name());
  if (!ipv6_sock) {
    MarkInvalid();
    return Status(Code::UNEXPECTED_FAILURE,
                  "Failed to initialize IPv6 ether socket");
  }
  ipv6_sock_ = std::move(ipv6_sock);

  status = ipv6_sock_->SetNonBlockingMode(true);
  if (!status) {
    MarkInvalid();
    return status;
  }

  status = ipv6_sock_->SetAllMulticastMode(true);
  if (!status) {
    MarkInvalid();
    return status;
  }

  status = ipv6_sock_->AttachFilter(&kNonNeighborDiscoveryFilter);
  // Attacted filter.
  if (!status) {
    MarkInvalid();
    return status;
  }

  // Open ICMPv6 socket.
  auto icmp_sock = ICMPv6Socket::Create(name());
  if (!icmp_sock) {
    MarkInvalid();
    return Status(Code::UNEXPECTED_FAILURE,
                  "Failed to initialize ICMPv6 socket");
  }
  icmp_sock_ = std::move(icmp_sock);

  // Set as non-blocking.
  status = icmp_sock_->SetNonBlockingMode(true);
  if (!status) {
    MarkInvalid();
    return status;
  }

  // Get link-layer address.
  LLAddress ll_address;
  status = icmp_sock_->GetLinkLayerAddress(&ll_address);
  if (!status) {
    MarkInvalid();
    return status;
  }
  ll_address_ = ll_address;

  // Get link-layer MTU.
  uint32_t mtu;
  status = icmp_sock_->GetLinkMTU(&mtu);
  if (!status) {
    MarkInvalid();
    return status;
  }
  mtu_ = mtu;

  // Attach filter to block all in-coming packets.  For now, the ICMP
  // socket is to be used only for sending messages.
  // Info on ICMP6_FILTER in RFC3542, section 3.2.
  struct icmp6_filter icmp6_filter;
  ICMP6_FILTER_SETBLOCKALL(&icmp6_filter);
  status = icmp_sock_->AttachFilter(&icmp6_filter);
  if (!status) {
    MarkInvalid();
    return status;
  }

  // Set hop limits.  See Linux manual ipv6(7).
  // Multicast.
  status = icmp_sock_->SetMulticastHopLimit(kProxiedHopLimit);
  if (!status) {
    MarkInvalid();
    return status;
  }

  // Unicast.
  status = icmp_sock_->SetMulticastHopLimit(kProxiedHopLimit);
  if (!status) {
    MarkInvalid();
    return status;
  }

  if (!InternalRefreshIPv6AddressList()) {
    MarkInvalid();
    return Status(Code::UNEXPECTED_FAILURE)
           << "Failed to refresh IP address list on interface " << name();
  }

  // Done.  Mark as disabled and groupless.
  state_ = State::kProxyDisabled;
  // Don't call disable callback.
  MarkGroupless(false);

  return Status();
}

// private.
void ProxyInterface::MarkInvalid() {
  state_ = State::kInvalid;
  CloseOpenedFds();
  mtu_ = 0;
}

void ProxyInterface::CloseOpenedFds() {
  if (nd_sock_ && nd_sock_->IsReady()) {
    nd_sock_->Close();
  }
  if (ipv6_sock_ && ipv6_sock_->IsReady()) {
    ipv6_sock_->Close();
  }
  if (icmp_sock_ && icmp_sock_->IsReady()) {
    icmp_sock_->Close();
  }
}

bool ProxyInterface::IsValid() const {
  return (State::kInvalid != state_);
}

int ProxyInterface::GetInterfaceIndex() const {
  if (IsInitialized()) {
    return nd_sock_->index();
  }
  return -1;
}

const string& ProxyInterface::name() const {
  return name_;
}

int ProxyInterface::GetNDFd() const {
  return nd_sock_ ? nd_sock_->fd() : -1;
}

int ProxyInterface::GetIPv6Fd() const {
  return ipv6_sock_ ? ipv6_sock_->fd() : -1;
}

int ProxyInterface::GetICMPFd() const {
  return icmp_sock_ ? icmp_sock_->fd() : -1;
}

// L3 Information.

bool ProxyInterface::RefreshIPv6AddressList() {
  if (!IsInitialized()) {
    return false;
  }
  return InternalRefreshIPv6AddressList();
}

// private.
bool ProxyInterface::InternalRefreshIPv6AddressList() {
  DCHECK(!name().empty());
  struct ifaddrs* if_addr_head;
  if (getifaddrs(&if_addr_head) < 0) {
    const int saved_errno = errno;
    LOG(ERROR) << "Failed to get if addresses: getifaddrs(): "
               << base::safe_strerror(saved_errno);
    return false;
  }

  ip_addresses_.clear();
  // Need to loop through all address across all interfaces.  Skipping
  // non-IPv6 address and addresses that are unrelated to this
  // interface.
  struct ifaddrs* if_addr_node = if_addr_head;
  while (if_addr_node) {
    if (if_addr_node->ifa_addr != nullptr &&
        if_addr_node->ifa_addr->sa_family == AF_INET6 &&
        name() == if_addr_node->ifa_name) {
      // Must use sizeof(struct sockaddr_in6) instead of
      // sizeof(if_addr_node->ifa_addr).  We know from the if statement
      // that the `ifa_addr' is an IPv6 socket address.
      IPAddress address(if_addr_node->ifa_addr, sizeof(struct sockaddr_in6));
      if (address.IsValid() && address.family() == IPAddress::kFamilyIPv6) {
        ip_addresses_.push_back(address);
      }
    }
    if_addr_node = if_addr_node->ifa_next;
  }
  freeifaddrs(if_addr_head);
  return true;
}

bool ProxyInterface::HasIPv6Address(const shill::IPAddress& address) const {
  return base::ContainsValue(ip_addresses_, address);
}

// Proxy State.

bool ProxyInterface::IsInitialized() const {
  return (State::kProxyEnabled == state_ || State::kProxyDisabled == state_);
}

bool ProxyInterface::IsEnabled() const {
  return (State::kProxyEnabled == state_);
}

bool ProxyInterface::EnableProxy() {
  if (!IsInitialized()) {
    LOG(WARNING) << "Cannot enable an uninitialized interface: " << name_;
    return false;
  }
  if (IsEnabled()) {
    return true;
  }

  // Add any code required to enable the interface here.
  state_ = State::kProxyEnabled;
  return true;
}

bool ProxyInterface::DisableProxy() {
  if (!IsInitialized()) {
    LOG(WARNING) << "Cannot disable an uninitialized interface: " << name_;
    return false;
  }
  if (!IsEnabled()) {
    return true;
  }

  // Add any code required to disable the interface here.
  state_ = State::kProxyDisabled;
  return true;
}

bool ProxyInterface::Deinitialize() {
  if (!IsInitialized()) {
    LOG(WARNING) << "Cannot deinitialize an uninitialized interface: "
                 << name();
    return false;
  }

  CloseOpenedFds();
  state_ = State::kDeinitialized;
  return true;
}

// Sending ND Messages.

Status ProxyInterface::ProxyNeighborDiscoveryMessage(
    IPv6EtherHeader header_fields,
    const LLAddress& destination_ll_address,
    NeighborDiscoveryMessage nd_message) {
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot proxy on an uninitialized interface: " << name();
  }
  // Header validation.
  DCHECK(destination_ll_address.IsValid())
      << "Destination link-layer address is invalid";
  DCHECK_EQ(IPAddress::kFamilyIPv6, header_fields.source_address.family())
      << "Source address must be IPv6";
  DCHECK_EQ(IPAddress::kFamilyIPv6, header_fields.destination_address.family())
      << "Destination address must be IPv6";
  if (IPv6AddressIsUnspecified(header_fields.destination_address)) {
    return Status(Code::INVALID_ARGUMENT,
                  "Cannot proxy to an unspecified destination address");
  }
  if (header_fields.next_header != IPPROTO_ICMPV6) {
    return Status(Code::INVALID_ARGUMENT,
                  "Cannot proxy a non ICMPv6 packet on the ND socket");
  }
  // ND Message validation.
  DCHECK(nd_message.IsValid()) << "ND message must be valid";

  const NeighborDiscoveryMessage::Type nd_type = nd_message.type();
  // If router advertisement, set the proxy bit.
  if (nd_type == NeighborDiscoveryMessage::kTypeRouterAdvert) {
    nd_message.SetProxyFlag(true);
  }

  header_fields.hop_limit = kProxiedHopLimit;

  // Link-layer modifications.
  header_fields.source_ll_address = ll_address();
  header_fields.destination_ll_address = destination_ll_address;

  if (nd_message.HasSourceLinkLayerAddress()) {
    LLAddress source_ll_address;
    nd_message.GetSourceLinkLayerAddress(0, &source_ll_address);
    if (!source_ll_address.IsMulticast()) {
      nd_message.SetSourceLinkLayerAddress(0, ll_address());
    }
  }

  if (nd_message.HasTargetLinkLayerAddress()) {
    LLAddress target_ll_address;
    nd_message.GetTargetLinkLayerAddress(0, &target_ll_address);
    if (!target_ll_address.IsMulticast()) {
      nd_message.SetTargetLinkLayerAddress(0, ll_address());
    }
  }

  // To calculate the checksum, the current value must be zero.
  nd_message.SetChecksum(0);
  uint16_t checksum = 0;
  const Status checksum_status = IPv6UpperLayerChecksum16(
      header_fields.source_address, header_fields.destination_address,
      IPPROTO_ICMPV6, nd_message.message(), &checksum);

  if (checksum_status) {
    nd_message.SetChecksum(~checksum);
  } else {
    LOG(WARNING) << checksum_status;
    // Setting the checksum to 0 indicates that the checksum is not set.
    nd_message.SetChecksum(0);
  }

  DCHECK(nd_sock_);
  Status send_status =
      nd_sock_->SendIPv6Packet(header_fields, nd_message.message());
  PORTIER_RETURN_ON_FAILURE(send_status)
      << "Failed to proxy ND message on interface " << name();
  return Status();
}

// Receiving ND Messages.

Status ProxyInterface::ReceiveNeighborDiscoveryMessage(
    IPv6EtherHeader* header_fields, NeighborDiscoveryMessage* nd_message) {
  DCHECK(header_fields)
      << "Must provide an ND Message output parameter to receive "
      << "ND messages on interface " << name_;
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot receive from an uninitialized interface " << name();
  }
  DCHECK(nd_sock_);
  ByteString payload;
  Status receive_status = nd_sock_->ReceiveIPv6Packet(header_fields, &payload);
  PORTIER_RETURN_ON_FAILURE(receive_status)
      << "Failed to receive ND message on if " << name();

  if (header_fields->hop_limit != kProxiedHopLimit) {
    // RFC 4861: A node MUST silently discard any received Router
    // Advertisement (section 6.1.2), Neighbor Solicitation (section 7.1.1),
    // Neighbor Advertisement (section 7.1.2) if the IP Hop Limit field does
    // not have a value of 255.
    return Status(Code::RESULT_UNAVAILABLE);
  }

  // This should have been caught by BPF filter.
  DCHECK_EQ(IPPROTO_ICMPV6, header_fields->next_header)
      << "Next header is not ICMPv6";

  if (payload.GetLength() < sizeof(struct icmp6_hdr)) {
    return Status(Code::MALFORMED_PACKET)
           << "Received ICMPv6 packet is smaller than ICMPv6 header";
  }
  const struct icmp6_hdr* icmp6_hdr =
      reinterpret_cast<const struct icmp6_hdr*>(payload.GetConstData());

  // Ensure that the ICMPv6 packet contains a proxyable ND message.  These
  // should have been filtered out by the BPF filter.
  DCHECK(
      NeighborDiscoveryMessage::kTypeRouterAdvert == icmp6_hdr->icmp6_type ||
      NeighborDiscoveryMessage::kTypeNeighborSolicit == icmp6_hdr->icmp6_type ||
      NeighborDiscoveryMessage::kTypeNeighborAdvert == icmp6_hdr->icmp6_type ||
      NeighborDiscoveryMessage::kTypeRedirect == icmp6_hdr->icmp6_type);
  DCHECK_EQ(icmp6_hdr->icmp6_code, 0);

  // Extract ND Message.
  *nd_message = NeighborDiscoveryMessage(payload);

  if (!nd_message->IsValid()) {
    return Status(Code::MALFORMED_PACKET)
           << "Failed to parse ND message packet";
  }
  return Status();
}

Status ProxyInterface::DiscardNeighborDiscoveryInput() {
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot discard from an uninitialized interface " << name_;
  }
  DCHECK(nd_sock_);
  return nd_sock_->DiscardPacket();
}

Status ProxyInterface::SendIPv6Packet(IPv6EtherHeader header_fields,
                                      const LLAddress& destination_ll_address,
                                      const ByteString& payload) {
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot proxy on an uninitialized interface: " << name();
  }
  // Header validation.
  DCHECK(destination_ll_address.IsValid())
      << "Destination link-layer address is invalid";
  DCHECK_EQ(IPAddress::kFamilyIPv6, header_fields.source_address.family())
      << "Source address must be IPv6";
  DCHECK_EQ(IPAddress::kFamilyIPv6, header_fields.destination_address.family())
      << "Destination address must be IPv6";
  if (IPv6AddressIsUnspecified(header_fields.destination_address)) {
    return Status(Code::INVALID_ARGUMENT)
           << "Cannot proxy to an unspecified destination address: " << name();
  }

  // Link-layer modification.
  header_fields.source_ll_address = ll_address();
  header_fields.destination_ll_address = destination_ll_address;

  DCHECK(ipv6_sock_);
  Status send_status = ipv6_sock_->SendIPv6Packet(header_fields, payload);
  PORTIER_RETURN_ON_FAILURE(send_status)
      << "Failed to proxy ND message on interface " << name();
  return Status();
}

Status ProxyInterface::ReceiveIPv6Packet(IPv6EtherHeader* header_fields,
                                         ByteString* payload) {
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot receive from an uninitialized interface " << name_;
  }
  DCHECK(ipv6_sock_);
  return ipv6_sock_->ReceiveIPv6Packet(header_fields, payload);
}

Status ProxyInterface::DiscardIPv6Input() {
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot discard from an uninitialized interface " << name_;
  }
  DCHECK(ipv6_sock_);
  return ipv6_sock_->DiscardPacket();
}

Status ProxyInterface::SendPacketTooBigMessage(
    const IPAddress& destination_address,
    uint32_t mtu,
    const IPv6EtherHeader& original_header,
    const ByteString& original_body) {
  if (!IsInitialized()) {
    return Status(Code::BAD_INTERNAL_STATE)
           << "Cannot send ICMPv6 Packet Too Big on an uninitialized interface "
           << name_;
  }
  DCHECK(icmp_sock_);
  return icmp_sock_->SendPacketTooBigMessage(destination_address, mtu,
                                             original_header, original_body);
}

}  // namespace portier

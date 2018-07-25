// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_ND_MSG_H_
#define PORTIER_ND_MSG_H_

// Standard C Library.

// Standard C++ Library.
#include <map>
#include <memory>
#include <string>
#include <vector>

// Chrome OS Library.
#include <base/time/time.h>
#include <shill/net/byte_string.h>
#include <shill/net/ip_address.h>

// Portier Library.
#include "portier/ll_address.h"

namespace portier {

// Neighbor Discovery ICMPv6 Message.
class NeighborDiscoveryMessage {
 public:
  // ND ICMPv6 type, as defined in RFC 4861.
  using Type = uint8_t;
  static const Type kTypeRouterSolicit;
  static const Type kTypeRouterAdvert;
  static const Type kTypeNeighborSolicit;
  static const Type kTypeNeighborAdvert;
  static const Type kTypeRedirect;

  // Neighbor Discovery Option Types.
  using OptionType = uint8_t;
  static const OptionType kOptionTypeSourceLinkLayerAddress;
  static const OptionType kOptionTypeTargetLinkLayerAddress;
  static const OptionType kOptionTypePrefixInformation;
  static const OptionType kOptionTypeRedirectHeader;
  static const OptionType kOptionTypeMTU;

  static std::string GetTypeName(Type type);
  static uint32_t GetTypeMinimumLength(Type type);
  static std::string GetOptionTypeName(OptionType opt_type);
  static uint32_t GetOptionTypeMinimumLength(OptionType opt_type);

  static NeighborDiscoveryMessage RouterSolicit();

  static NeighborDiscoveryMessage RouterAdvert(
      uint8_t cur_hop_limit,
      bool managed_flag,
      bool other_flag,
      bool proxy_flag,
      base::TimeDelta router_lifetime,
      base::TimeDelta reachable_time,
      base::TimeDelta retransmit_timer);

  static NeighborDiscoveryMessage NeighborSolicit(
      const shill::IPAddress& target_address);

  static NeighborDiscoveryMessage NeighborAdvert(
      bool router_flag,
      bool solicited_flag,
      bool override_flag,
      const shill::IPAddress& target_address);

  static NeighborDiscoveryMessage Redirect(
      const shill::IPAddress& target_address,
      const shill::IPAddress& destination_address);

  NeighborDiscoveryMessage();

  // Attempt to parse.
  explicit NeighborDiscoveryMessage(const shill::ByteString& raw_packet);

  // Getters.
  Type type() const { return type_; }
  const shill::ByteString& message() const { return message_; }

  // Is the provided packet a properly formated ND ICMPv6 packet.
  bool IsValid() const;

  // Checksum.
  // Returns the checksum in network byte order.
  bool GetChecksum(uint16_t* checksum) const;

  bool SetChecksum(uint16_t checksum);

  // RS related.

  // RA related.
  //  All the following will return false if the ND message is invalid
  //  or not of the correct type.

  // The default value for Hop Count field for packets which are forwarded
  // by the router.
  bool GetCurrentHopLimit(uint8_t* cur_hop_limit) const;

  // Indicates that nodes can use DHCPv6 for address assignment.
  bool GetManagedAddressConfigurationFlag(bool* managed_flag) const;

  // Indicates that there is additional configuration information
  // available from DCHPv6 protocol.
  bool GetOtherConfigurationFlag(bool* other_flag) const;

  // Indicates that this RA has been proxied.  If receiving a proxied
  // RA, RFC 4389 states that this RA should be dropped and not proxied.
  bool GetProxyFlag(bool* proxy_flag) const;
  // Modifies the value of the proxy bit in RA messages.  This should be
  // used in the implementation of ND Proxy if directly copying received RA
  // messages.
  bool SetProxyFlag(bool proxy_flag);

  // Lifetime associated with the default route.  0 implies it is
  // unspecified.
  bool GetRouterLifetime(base::TimeDelta* router_lifetime) const;

  // The time assumed by the router that a neighbor is reachable after
  // having received a reachability confirmation.  0 implies that it
  // is unspecified.
  bool GetReachableTime(base::TimeDelta* reachable_time) const;

  // Time between retransmitted NS messages.  0 implies that it is
  // unspecified.
  bool GetRetransmitTimer(base::TimeDelta* retransmit_timer) const;

  // NS related.

  // Used for broth NS, NA, and R.
  bool GetTargetAddress(shill::IPAddress* target_address) const;

  // NA related.

  // Indicates that the sending node is a router.
  bool GetRouterFlag(bool* router_flag) const;

  // Indicates that the NA is response to a NS.
  bool GetSolicitedFlag(bool* solicited_flag) const;

  // Indicates that Neichbor Cache entries should be overwritten
  // as a result of this NA.
  bool GetOverrideFlag(bool* override_flag) const;

  // R related.

  // ICMP field Destination Address, differs from the destination
  // addresss found in the IP fields.
  bool GetDestinationAddress(shill::IPAddress* destination_address) const;

  // Options

  // Check if the exists at least 1 instance of the option.
  bool HasOption(OptionType opt_type) const;

  // Count the number of occurances of the option.
  uint32_t OptionCount(OptionType opt_type) const;

  // Get the raw bytes of the option, including the type and length fields.
  bool GetRawOption(OptionType opt_type,
                    uint32_t opt_index,
                    shill::ByteString* raw_option) const;

  // Remove all options.
  void ClearOptions();

  // Supported Options.

  // Source link-layer address option.
  bool HasSourceLinkLayerAddress() const;
  bool GetSourceLinkLayerAddress(uint32_t opt_index,
                                 LLAddress* source_ll_address) const;
  bool SetSourceLinkLayerAddress(uint32_t opt_index,
                                 const LLAddress& source_ll_address);
  bool PushSourceLinkLayerAddress(const LLAddress& source_ll_address);

  // Target link-layer address option.
  bool HasTargetLinkLayerAddress() const;
  bool GetTargetLinkLayerAddress(uint32_t opt_index,
                                 LLAddress* target_ll_address) const;
  bool SetTargetLinkLayerAddress(uint32_t opt_index,
                                 const LLAddress& target_ll_address);
  bool PushTargetLinkLayerAddress(const LLAddress& target_ll_address);

  // Prefix information
  bool HasPrefixInformation() const;
  uint32_t PrefixInformationCount() const;
  bool GetPrefixLength(uint32_t opt_index, uint8_t* prefix_length) const;
  bool GetOnLinkFlag(uint32_t opt_index, bool* on_link_flag) const;
  bool GetAutonomousAddressConfigurationFlag(uint32_t opt_index,
                                             bool* autonomous_flag) const;
  bool GetPrefixValidLifetime(uint32_t opt_index,
                              base::TimeDelta* valid_lifetime) const;
  bool GetPrefixPreferredLifetime(uint32_t opt_index,
                                  base::TimeDelta* preferred_lifetime) const;
  bool GetPrefix(uint32_t opt_index, shill::IPAddress* prefix) const;
  bool PushPrefixInformation(uint8_t prefix_length,
                             bool on_link_flag,
                             bool autonomous_flag,
                             const base::TimeDelta& valid_lifetime,
                             const base::TimeDelta& preferred_lifetime,
                             const shill::IPAddress& prefix);

  // Redirected header option.
  bool HasRedirectedHeader() const;
  bool GetIpHeaderAndData(uint32_t opt_index,
                          shill::ByteString* ip_header_and_data) const;
  bool PushRedirectedHeader(const shill::ByteString& ip_header_and_data);

  // MTU option.
  bool HasMTU() const;
  bool GetMTU(uint32_t opt_index, uint32_t* mtu) const;
  bool PushMTU(uint32_t mtu);

  const uint8_t* GetConstData() const;
  size_t GetLength() const;

 private:
  // Gets the character bytes of the data.
  uint8_t* GetData();

  // Generic option methods for link-layer addresses.  Used for
  // both source and target ll address option methods.
  bool GetLinkLayerAddress(OptionType opt_type,
                           uint32_t opt_index,
                           LLAddress* ll_address) const;
  bool SetLinkLayerAddress(OptionType opt_type,
                           uint32_t opt_index,
                           const LLAddress& ll_address);
  bool PushLinkLayerAddress(OptionType opt_type, const LLAddress& ll_address);

  // Go through all the options of the packet and set the byte index of the
  // options in the |options_| map.  Returns false if there are any issues
  // with the options.
  bool IndexOptions();

  void AddOptionIndex(OptionType opt_type, uint32_t data_index);

  uint8_t* GetOptionPointer(OptionType opt_type, uint32_t opt_index);
  const uint8_t* GetConstOptionPointer(OptionType opt_type,
                                       uint32_t opt_index) const;

  // ND type.
  Type type_;

  // Raw bytes of ICMP.
  shill::ByteString message_;

  // Pointer to array of byte indexes of ND options.
  std::map<OptionType, std::vector<uint32_t>> options_;
};

}  // namespace portier

#endif  // PORTIER_ND_MSG_H_

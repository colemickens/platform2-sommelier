// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/nd_msg.h"

#include <netinet/icmp6.h>

#include <utility>

namespace portier {

// Type aliasing.
using std::vector;
using std::string;

using base::TimeDelta;

using shill::ByteString;
using shill::IPAddress;

using Type = NeighborDiscoveryMessage::Type;
using OptionType = NeighborDiscoveryMessage::OptionType;

// Static contstants.

// ND codes from netinet/icmp6.h, defined in RFC3542.
const Type NeighborDiscoveryMessage::kTypeRouterSolicit = ND_ROUTER_SOLICIT;
const Type NeighborDiscoveryMessage::kTypeRouterAdvert = ND_ROUTER_ADVERT;
const Type NeighborDiscoveryMessage::kTypeNeighborSolicit = ND_NEIGHBOR_SOLICIT;
const Type NeighborDiscoveryMessage::kTypeNeighborAdvert = ND_NEIGHBOR_ADVERT;
const Type NeighborDiscoveryMessage::kTypeRedirect = ND_REDIRECT;

const OptionType NeighborDiscoveryMessage::kOptionTypeSourceLinkLayerAddress =
    ND_OPT_SOURCE_LINKADDR;
const OptionType NeighborDiscoveryMessage::kOptionTypeTargetLinkLayerAddress =
    ND_OPT_TARGET_LINKADDR;
const OptionType NeighborDiscoveryMessage::kOptionTypePrefixInformation =
    ND_OPT_PREFIX_INFORMATION;
const OptionType NeighborDiscoveryMessage::kOptionTypeRedirectHeader =
    ND_OPT_REDIRECTED_HEADER;
const OptionType NeighborDiscoveryMessage::kOptionTypeMTU = ND_OPT_MTU;

// Local constants.
namespace {

constexpr uint32_t kTypeMinLengthRouterSolicit =
    sizeof(struct nd_router_solicit);
constexpr uint32_t kTypeMinLengthRouterAdvert = sizeof(struct nd_router_advert);
constexpr uint32_t kTypeMinLengthNeighborSolicit =
    sizeof(struct nd_neighbor_solicit);
constexpr uint32_t kTypeMinLengthNeighborAdvert =
    sizeof(struct nd_neighbor_advert);
constexpr uint32_t kTypeMinLengthRedirect = sizeof(struct nd_redirect);

// Link layer addresses are variable length.
constexpr uint32_t kOptionTypeMinLengthSourceLinkLayerAddress =
    sizeof(struct nd_opt_hdr);
constexpr uint32_t kOptionTypeMinLengthTargetLinkLayerAddress =
    sizeof(struct nd_opt_hdr);
constexpr uint32_t kOptionTypeMinLengthPrefixInformation =
    sizeof(struct nd_opt_prefix_info);
constexpr uint32_t kOptionTypeMinLengthRedirectHeader =
    sizeof(struct nd_opt_rd_hdr);
constexpr uint32_t kOptionTypeMinLengthMTU = sizeof(struct nd_opt_mtu);

// Option length to byte units.
constexpr uint32_t kBytesPerOptLen = 8;

// Route advertisement flags.
// RFC 4861 - Section 4.2.
constexpr uint8_t kRouterAdvertManagedBit = ND_RA_FLAG_MANAGED;
constexpr uint8_t kRouterAdvertOtherBit = ND_RA_FLAG_OTHER;
// RFC 4389 - Section 3.
constexpr uint8_t kRouterAdvertProxyBit = 0x04;
}  // namespace

// static
string NeighborDiscoveryMessage::GetTypeName(Type type) {
  switch (type) {
    case kTypeRouterSolicit:
      return "Router Solicitation";
    case kTypeRouterAdvert:
      return "Router Advertisement";
    case kTypeNeighborSolicit:
      return "Neighbor Solicitation";
    case kTypeNeighborAdvert:
      return "Neighbor Advertisement";
    case kTypeRedirect:
      return "Redirect";
    default:
      return "Unknown Type";
  }
}

// static
uint32_t NeighborDiscoveryMessage::GetTypeMinimumLength(Type type) {
  switch (type) {
    case kTypeRouterSolicit:
      return kTypeMinLengthRouterSolicit;
    case kTypeRouterAdvert:
      return kTypeMinLengthRouterAdvert;
    case kTypeNeighborSolicit:
      return kTypeMinLengthNeighborSolicit;
    case kTypeNeighborAdvert:
      return kTypeMinLengthNeighborAdvert;
    case kTypeRedirect:
      return kTypeMinLengthRedirect;
    default:
      return 0;
  }
}

// static
string NeighborDiscoveryMessage::GetOptionTypeName(OptionType opt_type) {
  switch (opt_type) {
    case kOptionTypeSourceLinkLayerAddress:
      return "Source Link-Layer-Address";
    case kOptionTypeTargetLinkLayerAddress:
      return "Target Link-Layer-Address";
    case kOptionTypePrefixInformation:
      return "Prefix Information";
    case kOptionTypeRedirectHeader:
      return "Redirect Header";
    case kOptionTypeMTU:
      return "MTU";
    default:
      return "Unknown Option Type";
  }
}

// static
uint32_t NeighborDiscoveryMessage::GetOptionTypeMinimumLength(
    OptionType opt_type) {
  switch (opt_type) {
    case kOptionTypeSourceLinkLayerAddress:
      return kOptionTypeMinLengthSourceLinkLayerAddress;
    case kOptionTypeTargetLinkLayerAddress:
      return kOptionTypeMinLengthTargetLinkLayerAddress;
    case kOptionTypePrefixInformation:
      return kOptionTypeMinLengthPrefixInformation;
    case kOptionTypeRedirectHeader:
      return kOptionTypeMinLengthRedirectHeader;
    case kOptionTypeMTU:
      return kOptionTypeMinLengthMTU;
    default:
      return 0;
  }
}

// Static Constructors.

NeighborDiscoveryMessage NeighborDiscoveryMessage::RouterSolicit() {
  NeighborDiscoveryMessage nd_msg;
  nd_msg.type_ = kTypeRouterSolicit;
  nd_msg.message_.Resize(kTypeMinLengthRouterSolicit);

  // Convert to raw.
  struct nd_router_solicit* nd_rs =
      reinterpret_cast<struct nd_router_solicit*>(nd_msg.GetData());
  memset(nd_rs, 0, kTypeMinLengthRouterSolicit);
  nd_rs->nd_rs_type = nd_msg.type();

  return nd_msg;
}

NeighborDiscoveryMessage NeighborDiscoveryMessage::RouterAdvert(
    uint8_t cur_hop_limit,
    bool managed_flag,
    bool other_flag,
    bool proxy_flag,
    TimeDelta router_lifetime,
    TimeDelta reachable_time,
    TimeDelta retransmit_timer) {
  NeighborDiscoveryMessage nd_msg;
  nd_msg.type_ = kTypeRouterAdvert;
  nd_msg.message_.Resize(kTypeMinLengthRouterAdvert);

  struct nd_router_advert* nd_ra =
      reinterpret_cast<struct nd_router_advert*>(nd_msg.GetData());
  memset(nd_ra, 0, kTypeMinLengthNeighborAdvert);

  // Row 0: ND type.
  nd_ra->nd_ra_type = nd_msg.type();

  // Row 1: Cur Hop Limit, M O flags, Router Lifetime (16-bits).
  nd_ra->nd_ra_curhoplimit = cur_hop_limit;
  nd_ra->nd_ra_flags_reserved =
      (managed_flag ? kRouterAdvertManagedBit : 0x00) |
      (other_flag ? kRouterAdvertOtherBit : 0x00) |
      (proxy_flag ? kRouterAdvertProxyBit : 0x00);
  nd_ra->nd_ra_router_lifetime =
      htons(static_cast<uint16_t>(router_lifetime.InSeconds()));

  // Row 2: Reachable Time (32-bits).
  nd_ra->nd_ra_reachable =
      htonl(static_cast<uint32_t>(reachable_time.InMilliseconds()));

  // Row 3: Retransmit Timer (32-bits).
  nd_ra->nd_ra_retransmit =
      htonl(static_cast<uint32_t>(retransmit_timer.InMilliseconds()));

  return nd_msg;
}

NeighborDiscoveryMessage NeighborDiscoveryMessage::NeighborSolicit(
    const IPAddress& target_address) {
  // Check that IP address is valid.
  if (target_address.family() != IPAddress::kFamilyIPv6) {
    LOG(ERROR) << "Cannot initialize with a non-IPv6 target address: "
               << target_address.ToString();
    return NeighborDiscoveryMessage();
  }

  NeighborDiscoveryMessage nd_msg;
  nd_msg.type_ = kTypeNeighborSolicit;
  nd_msg.message_.Resize(kTypeMinLengthNeighborSolicit);

  struct nd_neighbor_solicit* nd_ns =
      reinterpret_cast<struct nd_neighbor_solicit*>(nd_msg.GetData());
  memset(nd_ns, 0, kTypeMinLengthNeighborSolicit);

  // Row 0: ND type.
  nd_ns->nd_ns_type = nd_msg.type();
  // Row 1: Reserved.
  // Row 2-5: Target address.
  memcpy(&nd_ns->nd_ns_target, target_address.GetConstData(),
         target_address.GetLength());

  return nd_msg;
}

NeighborDiscoveryMessage NeighborDiscoveryMessage::NeighborAdvert(
    bool router_flag,
    bool solicited_flag,
    bool override_flag,
    const IPAddress& target_address) {
  // Check that IP address is valid.
  if (target_address.family() != IPAddress::kFamilyIPv6) {
    LOG(ERROR) << "Cannot initialize with a non-IPv6 target address: "
               << target_address.ToString();
    return NeighborDiscoveryMessage();
  }

  NeighborDiscoveryMessage nd_msg;
  nd_msg.type_ = kTypeNeighborAdvert;
  nd_msg.message_.Resize(kTypeMinLengthNeighborAdvert);

  struct nd_neighbor_advert* nd_na =
      reinterpret_cast<struct nd_neighbor_advert*>(nd_msg.GetData());
  memset(nd_na, 0, kTypeMinLengthNeighborAdvert);

  // Row 0: ND type.
  nd_na->nd_na_type = nd_msg.type();
  // Row 1: R S O flags
  nd_na->nd_na_flags_reserved = (router_flag ? ND_NA_FLAG_ROUTER : 0) |
                                (solicited_flag ? ND_NA_FLAG_SOLICITED : 0) |
                                (override_flag ? ND_NA_FLAG_OVERRIDE : 0);
  // Row 2-5: Target addresss.
  memcpy(&nd_na->nd_na_target, target_address.GetConstData(),
         target_address.GetLength());

  return nd_msg;
}

NeighborDiscoveryMessage NeighborDiscoveryMessage::Redirect(
    const IPAddress& target_address, const IPAddress& destination_address) {
  if (target_address.family() != IPAddress::kFamilyIPv6) {
    LOG(ERROR) << "Cannot initialize with a non-IPv6 target address: "
               << target_address.ToString();
    return NeighborDiscoveryMessage();
  }
  if (destination_address.family() != IPAddress::kFamilyIPv6) {
    LOG(ERROR) << "Cannot initialize with a non-IPv6 destination address: "
               << destination_address.ToString();
    return NeighborDiscoveryMessage();
  }

  NeighborDiscoveryMessage nd_msg;
  nd_msg.type_ = kTypeRedirect;
  nd_msg.message_.Resize(kTypeMinLengthRedirect);

  struct nd_redirect* nd_rd =
      reinterpret_cast<struct nd_redirect*>(nd_msg.GetData());
  memset(nd_rd, 0, kTypeMinLengthRedirect);

  // Row 0: ND type.
  nd_rd->nd_rd_type = nd_msg.type();
  // Row 1: Reserved.
  // Row 2-5: Target address.
  memcpy(&nd_rd->nd_rd_target, target_address.GetConstData(),
         target_address.GetLength());
  // Row 6-9: Destination address.
  memcpy(&nd_rd->nd_rd_dst, destination_address.GetConstData(),
         destination_address.GetLength());

  return nd_msg;
}

// Default constructor, private.

NeighborDiscoveryMessage::NeighborDiscoveryMessage() : type_(0) {}

NeighborDiscoveryMessage::NeighborDiscoveryMessage(const ByteString& raw_packet)
    : type_(0) {
  if (raw_packet.GetLength() == 0) {
    LOG(WARNING) << "ND packet is empty";
    return;
  }

  // Get the type
  Type type = raw_packet.GetConstData()[0];
  uint32_t exp_len = GetTypeMinimumLength(type);
  if (0 == exp_len) {
    LOG(ERROR) << "Unsupported ICMPv6 type " << static_cast<uint32_t>(type);
    return;
  } else if (raw_packet.GetLength() < exp_len) {
    LOG(ERROR) << "Expected length of a " << GetTypeName(type)
               << " ND packet should be at least " << exp_len << ", got "
               << raw_packet.GetLength();
    return;
  }
  type_ = type;

  message_ = raw_packet;
  if (!IndexOptions()) {
    // Issue with options.  Exact error should have been logged by
    // IndexOptions().
    type_ = 0;
    message_.Clear();
  }
}

bool NeighborDiscoveryMessage::IsValid() const {
  switch (type()) {
    case kTypeRouterSolicit:
    case kTypeRouterAdvert:
    case kTypeNeighborSolicit:
    case kTypeNeighborAdvert:
    case kTypeRedirect:
      break;
    default:
      // Not a supported type.
      return false;
  }

  if (GetLength() < GetTypeMinimumLength(type())) {
    // Length is too small.
    return false;
  }

  if ((GetLength() % 8) != 0) {
    // Packet does not align on proper 64-bit boundry.
    return false;
  }

  // TODO(sigquit): Create ND type specific validation.  Options should have
  // been validated upon indexing.

  return true;
}

// Checksum.

bool NeighborDiscoveryMessage::GetChecksum(uint16_t* checksum) const {
  if (!IsValid() || nullptr == checksum) {
    return false;
  }
  const struct icmp6_hdr* icmp6_hdr =
      reinterpret_cast<const struct icmp6_hdr*>(GetConstData());
  *checksum = icmp6_hdr->icmp6_cksum;
  return true;
}

bool NeighborDiscoveryMessage::SetChecksum(uint16_t checksum) {
  if (!IsValid()) {
    return false;
  }
  struct icmp6_hdr* icmp6_hdr = reinterpret_cast<struct icmp6_hdr*>(GetData());
  icmp6_hdr->icmp6_cksum = checksum;
  return true;
}

// RS related.

// RA related.

bool NeighborDiscoveryMessage::GetCurrentHopLimit(
    uint8_t* cur_hop_limit) const {
  if (!IsValid() || type() != kTypeRouterAdvert || nullptr == cur_hop_limit) {
    // Not a router advert.
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *cur_hop_limit = nd_ra->nd_ra_curhoplimit;
  return true;
}

bool NeighborDiscoveryMessage::GetManagedAddressConfigurationFlag(
    bool* managed_flag) const {
  if (!IsValid() || type() != kTypeRouterAdvert || nullptr == managed_flag) {
    // Not a router advert.
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *managed_flag = (nd_ra->nd_ra_flags_reserved & kRouterAdvertManagedBit) != 0;
  return true;
}

bool NeighborDiscoveryMessage::GetOtherConfigurationFlag(
    bool* other_flag) const {
  if (!IsValid() || type() != kTypeRouterAdvert || nullptr == other_flag) {
    // Not a router advert.
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *other_flag = (nd_ra->nd_ra_flags_reserved & kRouterAdvertOtherBit) != 0;
  return true;
}

bool NeighborDiscoveryMessage::GetProxyFlag(bool* proxy_flag) const {
  if (!IsValid() || type() != kTypeRouterAdvert || nullptr == proxy_flag) {
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *proxy_flag = (nd_ra->nd_ra_flags_reserved & kRouterAdvertProxyBit) != 0;
  return true;
}

bool NeighborDiscoveryMessage::SetProxyFlag(bool proxy_flag) {
  if (!IsValid() || type() != kTypeRouterAdvert) {
    return false;
  }
  struct nd_router_advert* nd_ra =
      reinterpret_cast<struct nd_router_advert*>(GetData());
  if (proxy_flag) {
    nd_ra->nd_ra_flags_reserved |= kRouterAdvertProxyBit;
  } else {
    nd_ra->nd_ra_flags_reserved &= ~kRouterAdvertProxyBit;
  }
  return true;
}

bool NeighborDiscoveryMessage::GetRouterLifetime(
    TimeDelta* router_lifetime) const {
  if (!IsValid() || type() != kTypeRouterAdvert || nullptr == router_lifetime) {
    // Not a router advert.
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *router_lifetime =
      TimeDelta::FromSeconds(ntohs(nd_ra->nd_ra_router_lifetime));
  return true;
}

bool NeighborDiscoveryMessage::GetReachableTime(
    TimeDelta* reachable_time) const {
  if (!IsValid() || type() != kTypeRouterAdvert || nullptr == reachable_time) {
    // Not a router advert.
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *reachable_time = TimeDelta::FromMilliseconds(ntohl(nd_ra->nd_ra_reachable));
  return true;
}

bool NeighborDiscoveryMessage::GetRetransmitTimer(
    TimeDelta* retransmit_timer) const {
  if (!IsValid() || type() != kTypeRouterAdvert ||
      nullptr == retransmit_timer) {
    // Not a router advert.
    return false;
  }
  const struct nd_router_advert* nd_ra =
      reinterpret_cast<const struct nd_router_advert*>(GetConstData());
  *retransmit_timer =
      TimeDelta::FromMilliseconds(ntohl(nd_ra->nd_ra_retransmit));
  return true;
}

// NS related.

bool NeighborDiscoveryMessage::GetTargetAddress(
    IPAddress* target_address) const {
  if (!IsValid() ||
      (type() != kTypeNeighborSolicit && type() != kTypeNeighborAdvert &&
       type() != kTypeRedirect) ||
      nullptr == target_address) {
    // Not a NS, NA or R
    return false;
  }

  if (type() == kTypeNeighborSolicit) {
    // Neighbor Solicit.
    const struct nd_neighbor_solicit* nd_ns =
        reinterpret_cast<const struct nd_neighbor_solicit*>(GetConstData());
    *target_address = IPAddress(
        IPAddress::kFamilyIPv6,
        ByteString(reinterpret_cast<const uint8_t*>(&nd_ns->nd_ns_target),
                   sizeof(nd_ns->nd_ns_target)));
    return true;
  }

  if (type() == kTypeNeighborAdvert) {
    // Neighbor Advert.
    const struct nd_neighbor_advert* nd_na =
        reinterpret_cast<const struct nd_neighbor_advert*>(GetConstData());
    *target_address = IPAddress(
        IPAddress::kFamilyIPv6,
        ByteString(reinterpret_cast<const uint8_t*>(&nd_na->nd_na_target),
                   sizeof(nd_na->nd_na_target)));
    return true;
  }

  // Redirect.
  const struct nd_redirect* nd_rd =
      reinterpret_cast<const struct nd_redirect*>(GetConstData());
  *target_address = IPAddress(
      IPAddress::kFamilyIPv6,
      ByteString(reinterpret_cast<const uint8_t*>(&nd_rd->nd_rd_target),
                 sizeof(nd_rd->nd_rd_target)));
  return true;
}

// NA related.

bool NeighborDiscoveryMessage::GetRouterFlag(bool* router_flag) const {
  if (!IsValid() || type() != kTypeNeighborAdvert || nullptr == router_flag) {
    // Not a NA.
    return false;
  }

  const struct nd_neighbor_advert* nd_na =
      reinterpret_cast<const struct nd_neighbor_advert*>(GetConstData());
  *router_flag = (nd_na->nd_na_flags_reserved & ND_NA_FLAG_ROUTER) != 0;
  return true;
}

bool NeighborDiscoveryMessage::GetSolicitedFlag(bool* solicited_flag) const {
  if (!IsValid() || type() != kTypeNeighborAdvert ||
      nullptr == solicited_flag) {
    // Not a NA.
    return false;
  }

  const struct nd_neighbor_advert* nd_na =
      reinterpret_cast<const struct nd_neighbor_advert*>(GetConstData());
  *solicited_flag = (nd_na->nd_na_flags_reserved & ND_NA_FLAG_SOLICITED) != 0;
  return true;
}

bool NeighborDiscoveryMessage::GetOverrideFlag(bool* override_flag) const {
  if (!IsValid() || type() != kTypeNeighborAdvert || nullptr == override_flag) {
    // Not a NA.
    return false;
  }

  const struct nd_neighbor_advert* nd_na =
      reinterpret_cast<const struct nd_neighbor_advert*>(GetConstData());
  *override_flag = (nd_na->nd_na_flags_reserved & ND_NA_FLAG_OVERRIDE) != 0;
  return true;
}

// Redirect related.

bool NeighborDiscoveryMessage::GetDestinationAddress(
    IPAddress* destination_address) const {
  if (!IsValid() || type() != kTypeRedirect || nullptr == destination_address) {
    // Not a Redirect.
    return false;
  }

  const struct nd_redirect* nd_rd =
      reinterpret_cast<const struct nd_redirect*>(GetConstData());
  *destination_address =
      IPAddress(IPAddress::kFamilyIPv6,
                ByteString(reinterpret_cast<const uint8_t*>(&nd_rd->nd_rd_dst),
                           sizeof(nd_rd->nd_rd_dst)));
  return true;
}

// Raw accessors.
const uint8_t* NeighborDiscoveryMessage::GetConstData() const {
  return message_.GetConstData();
}

// private
uint8_t* NeighborDiscoveryMessage::GetData() {
  return message_.GetData();
}

size_t NeighborDiscoveryMessage::GetLength() const {
  return message_.GetLength();
}

// Options.

namespace {

// Helpers for options without a well-define structure.
const uint8_t* OptionConstData(const struct nd_opt_hdr* opt_hdr) {
  return &(
      reinterpret_cast<const uint8_t*>(opt_hdr)[sizeof(struct nd_opt_hdr)]);
}

uint8_t* OptionData(struct nd_opt_hdr* opt_hdr) {
  return &(reinterpret_cast<uint8_t*>(opt_hdr)[sizeof(struct nd_opt_hdr)]);
}

uint32_t OptionDataLength(const struct nd_opt_hdr* opt_hdr) {
  return (opt_hdr->nd_opt_len * kBytesPerOptLen) - sizeof(struct nd_opt_hdr);
}

}  // namespace

bool NeighborDiscoveryMessage::HasOption(OptionType opt_type) const {
  if (!IsValid()) {
    return false;
  }
  return OptionCount(opt_type) > 0;
}

uint32_t NeighborDiscoveryMessage::OptionCount(OptionType opt_type) const {
  if (!IsValid()) {
    return 0;
  }

  auto opt_it = options_.find(opt_type);
  if (opt_it == options_.cend()) {
    // No occurance.
    return 0;
  }
  return opt_it->second.size();
}

bool NeighborDiscoveryMessage::GetRawOption(OptionType opt_type,
                                            uint32_t opt_index,
                                            ByteString* raw_option) const {
  if (opt_index >= OptionCount(opt_type) || nullptr == raw_option) {
    return false;
  }

  const uint8_t* opt_ptr = GetConstOptionPointer(opt_type, opt_index);
  CHECK(nullptr != opt_ptr);

  // Determine the length of the option.
  const struct nd_opt_hdr* opt_hdr =
      reinterpret_cast<const struct nd_opt_hdr*>(opt_ptr);
  CHECK(opt_hdr->nd_opt_type == opt_type);
  const uint32_t opt_length = (opt_hdr->nd_opt_len * kBytesPerOptLen);

  *raw_option = ByteString(opt_ptr, opt_length);
  return true;
}

void NeighborDiscoveryMessage::ClearOptions() {
  if (!IsValid()) {
    return;
  }
  // Clear indexes.
  options_.clear();
  // Truncate message.
  message_.Resize(GetTypeMinimumLength(type()));
}

// Internal option accessor.

uint8_t* NeighborDiscoveryMessage::GetOptionPointer(OptionType opt_type,
                                                    uint32_t opt_index) {
  return const_cast<uint8_t*>(GetConstOptionPointer(opt_type, opt_index));
}

const uint8_t* NeighborDiscoveryMessage::GetConstOptionPointer(
    OptionType opt_type, uint32_t opt_index) const {
  // Verify sanity
  CHECK(IsValid());
  CHECK(opt_index < OptionCount(opt_type));

  auto opt_it = options_.find(opt_type);
  uint32_t data_idx = opt_it->second[opt_index];

  CHECK(data_idx < GetLength());
  return &message_.GetConstData()[data_idx];
}

// Option - Source link-layer address.

bool NeighborDiscoveryMessage::HasSourceLinkLayerAddress() const {
  return HasOption(kOptionTypeSourceLinkLayerAddress);
}

bool NeighborDiscoveryMessage::GetSourceLinkLayerAddress(
    uint32_t opt_index, LLAddress* source_ll_address) const {
  return GetLinkLayerAddress(kOptionTypeSourceLinkLayerAddress, opt_index,
                             source_ll_address);
}

bool NeighborDiscoveryMessage::SetSourceLinkLayerAddress(
    uint32_t opt_index, const LLAddress& source_ll_address) {
  return SetLinkLayerAddress(kOptionTypeSourceLinkLayerAddress, opt_index,
                             source_ll_address);
}

bool NeighborDiscoveryMessage::PushSourceLinkLayerAddress(
    const LLAddress& source_ll_address) {
  return PushLinkLayerAddress(kOptionTypeSourceLinkLayerAddress,
                              source_ll_address);
}

// Option - Target link-layer address.

bool NeighborDiscoveryMessage::HasTargetLinkLayerAddress() const {
  return HasOption(kOptionTypeTargetLinkLayerAddress);
}

bool NeighborDiscoveryMessage::GetTargetLinkLayerAddress(
    uint32_t opt_index, LLAddress* target_ll_address) const {
  return GetLinkLayerAddress(kOptionTypeTargetLinkLayerAddress, opt_index,
                             target_ll_address);
}

bool NeighborDiscoveryMessage::SetTargetLinkLayerAddress(
    uint32_t opt_index, const LLAddress& target_ll_address) {
  return SetLinkLayerAddress(kOptionTypeTargetLinkLayerAddress, opt_index,
                             target_ll_address);
}

bool NeighborDiscoveryMessage::PushTargetLinkLayerAddress(
    const LLAddress& target_ll_address) {
  return PushLinkLayerAddress(kOptionTypeTargetLinkLayerAddress,
                              target_ll_address);
}

// Option - Prefix information.

bool NeighborDiscoveryMessage::HasPrefixInformation() const {
  return HasOption(kOptionTypePrefixInformation);
}

uint32_t NeighborDiscoveryMessage::PrefixInformationCount() const {
  return OptionCount(kOptionTypePrefixInformation);
}

bool NeighborDiscoveryMessage::GetPrefixLength(uint32_t opt_index,
                                               uint8_t* prefix_length) const {
  if (opt_index >= PrefixInformationCount() || nullptr == prefix_length) {
    return false;
  }

  const struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<const struct nd_opt_prefix_info*>(
          GetConstOptionPointer(kOptionTypePrefixInformation, opt_index));

  CHECK_EQ(opt_prefix_info->nd_opt_pi_type, kOptionTypePrefixInformation);

  *prefix_length = opt_prefix_info->nd_opt_pi_prefix_len;
  return true;
}

bool NeighborDiscoveryMessage::GetOnLinkFlag(uint32_t opt_index,
                                             bool* on_link_flag) const {
  if (opt_index >= PrefixInformationCount() || nullptr == on_link_flag) {
    return false;
  }

  const struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<const struct nd_opt_prefix_info*>(
          GetConstOptionPointer(kOptionTypePrefixInformation, opt_index));

  CHECK_EQ(opt_prefix_info->nd_opt_pi_type, kOptionTypePrefixInformation);

  *on_link_flag = ((opt_prefix_info->nd_opt_pi_flags_reserved &
                    ND_OPT_PI_FLAG_ONLINK) != 0);
  return true;
}

bool NeighborDiscoveryMessage::GetAutonomousAddressConfigurationFlag(
    uint32_t opt_index, bool* autonomous_flag) const {
  if (opt_index >= PrefixInformationCount() || nullptr == autonomous_flag) {
    return false;
  }

  const struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<const struct nd_opt_prefix_info*>(
          GetConstOptionPointer(kOptionTypePrefixInformation, opt_index));

  CHECK_EQ(opt_prefix_info->nd_opt_pi_type, kOptionTypePrefixInformation);

  *autonomous_flag =
      ((opt_prefix_info->nd_opt_pi_flags_reserved & ND_OPT_PI_FLAG_AUTO) != 0);
  return true;
}

bool NeighborDiscoveryMessage::GetPrefixValidLifetime(
    uint32_t opt_index, TimeDelta* valid_lifetime) const {
  if (opt_index >= PrefixInformationCount() || nullptr == valid_lifetime) {
    return false;
  }

  const struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<const struct nd_opt_prefix_info*>(
          GetConstOptionPointer(kOptionTypePrefixInformation, opt_index));

  CHECK_EQ(opt_prefix_info->nd_opt_pi_type, kOptionTypePrefixInformation);

  *valid_lifetime =
      TimeDelta::FromSeconds(ntohl(opt_prefix_info->nd_opt_pi_valid_time));
  return true;
}

bool NeighborDiscoveryMessage::GetPrefixPreferredLifetime(
    uint32_t opt_index, TimeDelta* preferred_lifetime) const {
  if (opt_index >= PrefixInformationCount() || nullptr == preferred_lifetime) {
    return false;
  }

  const struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<const struct nd_opt_prefix_info*>(
          GetConstOptionPointer(kOptionTypePrefixInformation, opt_index));

  CHECK_EQ(opt_prefix_info->nd_opt_pi_type, kOptionTypePrefixInformation);

  *preferred_lifetime =
      TimeDelta::FromSeconds(ntohl(opt_prefix_info->nd_opt_pi_preferred_time));
  return true;
}

bool NeighborDiscoveryMessage::GetPrefix(uint32_t opt_index,
                                         IPAddress* prefix) const {
  if (opt_index >= PrefixInformationCount() || nullptr == prefix) {
    return false;
  }

  const struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<const struct nd_opt_prefix_info*>(
          GetConstOptionPointer(kOptionTypePrefixInformation, opt_index));

  CHECK_EQ(opt_prefix_info->nd_opt_pi_type, kOptionTypePrefixInformation);

  ByteString prefix_bytes(
      reinterpret_cast<const uint8_t*>(&opt_prefix_info->nd_opt_pi_prefix),
      sizeof(opt_prefix_info->nd_opt_pi_prefix));

  // Convert, but validate before returning.
  IPAddress temp_prefix(IPAddress::kFamilyIPv6, prefix_bytes);

  if (!temp_prefix.IsValid()) {
    return false;
  }

  *prefix = temp_prefix;
  return true;
}

bool NeighborDiscoveryMessage::PushPrefixInformation(
    uint8_t prefix_length,
    bool on_link_flag,
    bool autonomous_flag,
    const TimeDelta& valid_lifetime,
    const TimeDelta& preferred_lifetime,
    const IPAddress& prefix) {
  if (!IsValid() || !prefix.IsValid() ||
      prefix.family() != IPAddress::kFamilyIPv6) {
    return false;
  }

  ByteString opt_buf(kOptionTypeMinLengthPrefixInformation);
  memset(opt_buf.GetData(), 0, opt_buf.GetLength());

  struct nd_opt_prefix_info* opt_prefix_info =
      reinterpret_cast<struct nd_opt_prefix_info*>(opt_buf.GetData());

  // Transfer data to struct.
  opt_prefix_info->nd_opt_pi_type = kOptionTypePrefixInformation;

  opt_prefix_info->nd_opt_pi_len =
      kOptionTypeMinLengthPrefixInformation / kBytesPerOptLen;
  CHECK_EQ(opt_prefix_info->nd_opt_pi_len * kBytesPerOptLen,
           kOptionTypeMinLengthPrefixInformation);

  opt_prefix_info->nd_opt_pi_prefix_len = prefix_length;

  opt_prefix_info->nd_opt_pi_flags_reserved =
      (on_link_flag ? ND_OPT_PI_FLAG_ONLINK : 0x00) |
      (autonomous_flag ? ND_OPT_PI_FLAG_AUTO : 0x00);

  opt_prefix_info->nd_opt_pi_valid_time =
      htonl(static_cast<uint32_t>(valid_lifetime.InSeconds()));

  opt_prefix_info->nd_opt_pi_preferred_time =
      htonl(static_cast<uint32_t>(preferred_lifetime.InSeconds()));

  memcpy(reinterpret_cast<uint8_t*>(&opt_prefix_info->nd_opt_pi_prefix),
         prefix.GetConstData(), sizeof(opt_prefix_info->nd_opt_pi_prefix));

  // Add to the current message, and index new option.
  const uint32_t data_index = GetLength();
  message_.Append(opt_buf);
  AddOptionIndex(kOptionTypePrefixInformation, data_index);
  return true;
}

// Option - Redirect header.

bool NeighborDiscoveryMessage::HasRedirectedHeader() const {
  return HasOption(kOptionTypeRedirectHeader);
}

bool NeighborDiscoveryMessage::GetIpHeaderAndData(
    uint32_t opt_index, ByteString* ip_header_and_data) const {
  if (opt_index >= OptionCount(kOptionTypeRedirectHeader) ||
      nullptr == ip_header_and_data) {
    return false;
  }

  const uint8_t* opt_ptr =
      GetConstOptionPointer(kOptionTypeRedirectHeader, opt_index);
  const struct nd_opt_rd_hdr* opt_rd_hdr =
      reinterpret_cast<const struct nd_opt_rd_hdr*>(opt_ptr);

  CHECK_EQ(opt_rd_hdr->nd_opt_rh_type, kOptionTypeRedirectHeader);

  const uint32_t header_length = (opt_rd_hdr->nd_opt_rh_len * kBytesPerOptLen) -
                                 sizeof(struct nd_opt_rd_hdr);

  if (header_length == 0) {
    // Nothing to returns.
    ip_header_and_data->Clear();
    return true;
  }

  *ip_header_and_data =
      ByteString(&opt_ptr[sizeof(struct nd_opt_rd_hdr)], header_length);
  return true;
}

bool NeighborDiscoveryMessage::PushRedirectedHeader(
    const shill::ByteString& ip_header_and_data) {
  if (!IsValid()) {
    return false;
  }

  // Allocate space for the option header.
  ByteString opt_buf(kOptionTypeMinLengthRedirectHeader);
  memset(opt_buf.GetData(), 0, opt_buf.GetLength());
  struct nd_opt_rd_hdr* opt_rd_hdr =
      reinterpret_cast<struct nd_opt_rd_hdr*>(opt_buf.GetData());

  opt_rd_hdr->nd_opt_rh_type = kOptionTypeRedirectHeader;
  opt_rd_hdr->nd_opt_rh_len =
      (kOptionTypeMinLengthRedirectHeader / kBytesPerOptLen) +
      (ip_header_and_data.GetLength() / kBytesPerOptLen) +
      // Possible that there is an incomplete 64-bit block.
      ((ip_header_and_data.GetLength() % kBytesPerOptLen) != 0 ? 1 : 0);

  opt_rd_hdr = nullptr;  // Appending can invalidate the header pointer.
  opt_buf.Append(ip_header_and_data);

  // Add padding as needed.
  if ((opt_buf.GetLength() % kBytesPerOptLen) != 0) {
    ByteString pad(kBytesPerOptLen - (opt_buf.GetLength() % kBytesPerOptLen));
    memset(pad.GetData(), 0, pad.GetLength());
    opt_buf.Append(pad);
  }

  // Append option and index it.
  const uint32_t data_index = GetLength();
  message_.Append(opt_buf);
  AddOptionIndex(kOptionTypeRedirectHeader, data_index);
  return true;
}

// Option - MTU

bool NeighborDiscoveryMessage::HasMTU() const {
  return HasOption(kOptionTypeMTU);
}

bool NeighborDiscoveryMessage::GetMTU(uint32_t opt_index, uint32_t* mtu) const {
  if (opt_index >= OptionCount(kOptionTypeMTU) || nullptr == mtu) {
    return false;
  }

  const struct nd_opt_mtu* opt_mtu = reinterpret_cast<const struct nd_opt_mtu*>(
      GetConstOptionPointer(kOptionTypeMTU, opt_index));

  CHECK_EQ(opt_mtu->nd_opt_mtu_type, kOptionTypeMTU);

  *mtu = ntohl(opt_mtu->nd_opt_mtu_mtu);
  return true;
}

bool NeighborDiscoveryMessage::PushMTU(uint32_t mtu) {
  if (!IsValid()) {
    return false;
  }

  ByteString opt_buf(kOptionTypeMinLengthMTU);
  memset(opt_buf.GetData(), 0, opt_buf.GetLength());
  struct nd_opt_mtu* opt_mtu =
      reinterpret_cast<struct nd_opt_mtu*>(opt_buf.GetData());

  opt_mtu->nd_opt_mtu_type = kOptionTypeMTU;
  opt_mtu->nd_opt_mtu_len = 1;
  opt_mtu->nd_opt_mtu_mtu = htonl(mtu);

  const uint32_t data_index = GetLength();
  message_.Append(opt_buf);
  AddOptionIndex(kOptionTypeMTU, data_index);
  return true;
}

// Option - Generic link-layer address (internal).

bool NeighborDiscoveryMessage::GetLinkLayerAddress(
    OptionType opt_type, uint32_t opt_index, LLAddress* ll_address) const {
  CHECK(kOptionTypeSourceLinkLayerAddress == opt_type ||
        kOptionTypeTargetLinkLayerAddress == opt_type);

  if (nullptr == ll_address || opt_index >= OptionCount(opt_type)) {
    return false;
  }

  const uint8_t* opt_ptr = GetConstOptionPointer(opt_type, opt_index);
  const struct nd_opt_hdr* opt_hdr =
      reinterpret_cast<const struct nd_opt_hdr*>(opt_ptr);

  CHECK_EQ(opt_hdr->nd_opt_type, opt_type);

  ByteString opt_data(OptionConstData(opt_hdr), OptionDataLength(opt_hdr));

  if (opt_data.GetLength() !=
      LLAddress::GetTypeLength(LLAddress::Type::kEui48)) {
    // Unknown length of EUI
    return false;
  }

  *ll_address = LLAddress(LLAddress::Type::kEui48, opt_data);
  return true;
}

// private
bool NeighborDiscoveryMessage::SetLinkLayerAddress(
    OptionType opt_type, uint32_t opt_index, const LLAddress& ll_address) {
  CHECK(kOptionTypeSourceLinkLayerAddress == opt_type ||
        kOptionTypeTargetLinkLayerAddress == opt_type);

  if (opt_index >= OptionCount(opt_type) || !ll_address.IsValid()) {
    return false;
  }
  if (ll_address.type() != LLAddress::Type::kEui48) {
    NOTIMPLEMENTED() << "Current can't handle non-Ethernet link-layer "
                     << "addresses, got "
                     << LLAddress::GetTypeName(ll_address.type());
    return false;
  }

  uint8_t* opt_ptr = GetOptionPointer(opt_type, opt_index);
  struct nd_opt_hdr* opt_hdr = reinterpret_cast<struct nd_opt_hdr*>(opt_ptr);

  if (ll_address.GetLength() != OptionDataLength(opt_hdr)) {
    // Currently cannot handle the case where the stored link-layer
    // address is a different size than the one replacing it.
    return false;
  }

  memcpy(OptionData(opt_hdr), ll_address.GetConstData(),
         ll_address.GetLength());
  return true;
}

// private
bool NeighborDiscoveryMessage::PushLinkLayerAddress(
    OptionType opt_type, const LLAddress& ll_address) {
  CHECK(kOptionTypeSourceLinkLayerAddress == opt_type ||
        kOptionTypeTargetLinkLayerAddress == opt_type);

  if (!IsValid()) {
    return false;
  }
  if (ll_address.type() != LLAddress::Type::kEui48) {
    NOTIMPLEMENTED() << "Current can't handle non-Ethernet link-layer "
                     << "addresses, got "
                     << LLAddress::GetTypeName(ll_address.type());
    return false;
  }

  // Allocate space for the option.
  ByteString opt_buf(kBytesPerOptLen);
  struct nd_opt_hdr* opt_hdr =
      reinterpret_cast<struct nd_opt_hdr*>(opt_buf.GetData());

  // Populate the option.
  opt_hdr->nd_opt_type = opt_type;
  opt_hdr->nd_opt_len = 1;
  memcpy(OptionData(opt_hdr), ll_address.GetConstData(),
         ll_address.GetLength());

  // Append to the current message and update the index.
  const uint32_t data_index = GetLength();
  message_.Append(opt_buf);
  AddOptionIndex(opt_type, data_index);
  return true;
}

// Indexing Options.

bool NeighborDiscoveryMessage::IndexOptions() {
  const uint32_t min_len = GetTypeMinimumLength(type());
  CHECK_GT(min_len, 0);
  CHECK(GetLength() >= min_len);

  // Clear all of the current indexes.
  options_.clear();

  // If there are none, then return.
  if (GetLength() == min_len) {
    // No options, done.
    return true;
  }

  uint32_t bytes_remaining = GetLength() - min_len;
  if ((bytes_remaining % kBytesPerOptLen) != 0) {
    // Packet does not align on 64-bit boundaries.
    return false;
  }

  uint32_t data_index = min_len;
  do {
    const uint8_t* opt_ptr = &GetConstData()[data_index];
    const struct nd_opt_hdr* opt_hdr =
        reinterpret_cast<const struct nd_opt_hdr*>(opt_ptr);
    const uint32_t opt_len = (opt_hdr->nd_opt_len * kBytesPerOptLen);

    // Check if invalid option length.  Packet might have been received using
    // AF_PACKET which does not validate ICMPv6 frames in the kernel before
    // passing up to user-space.
    if (0 == opt_len) {
      // RFC 4861: Nodes MUST silently discard an ND packet that contains an
      // option with length zero.
      LOG(ERROR) << "Received option with zero-length option: "
                 << GetOptionTypeName(opt_hdr->nd_opt_type) << " ("
                 << static_cast<uint32_t>(opt_hdr->nd_opt_type) << ")";
      options_.clear();
      return false;
    }

    if (opt_len > bytes_remaining) {
      // Not possible unless packet was truncated.
      LOG(ERROR) << "Option length is greater than remaining packet size";
      options_.clear();
      return false;
    }

    const OptionType opt_type = opt_hdr->nd_opt_type;
    const uint32_t opt_min_len = GetOptionTypeMinimumLength(opt_type);

    if (opt_min_len == 0) {
      // RFC 4861: Receivers MUST silently ignore any options they do not
      // recognize and continue processing the message.
      LOG(WARNING) << "Indexing unknown option type "
                   << static_cast<uint32_t>(opt_type);
      AddOptionIndex(opt_type, data_index);
    } else {
      switch (opt_type) {
        case kOptionTypeSourceLinkLayerAddress:
        case kOptionTypeTargetLinkLayerAddress:
          // Technically, the LL address can be any length necessary, so
          // it we must assume it is valid.
          AddOptionIndex(opt_type, data_index);
          break;
        case kOptionTypePrefixInformation:
        case kOptionTypeRedirectHeader:
        case kOptionTypeMTU:
          // These options have fixed lengths and are invalid if their size
          // does not match the expected length.  RFC 4861 does not specify
          // how this is to be handled, for now, we silently ignore the option,
          // but we do not index it.
          if (opt_len >= opt_min_len) {
            AddOptionIndex(opt_type, data_index);
          }
          break;
        default:
          // Should not reach this branch.
          CHECK(false);
      }
    }
    data_index += opt_len;
    bytes_remaining -= opt_len;
  } while (bytes_remaining > 0);

  return true;
}

void NeighborDiscoveryMessage::AddOptionIndex(OptionType opt_type,
                                              uint32_t data_index) {
  using OptPair = std::pair<OptionType, vector<uint32_t>>;

  CHECK(GetLength() > GetTypeMinimumLength(type()));

  // If index vector does not exsts, create new index vector for option type.
  if (options_.find(opt_type) == options_.end()) {
    options_.insert(OptPair(opt_type, vector<uint32_t>()));
  }
  options_[opt_type].push_back(data_index);
}

}  // namespace portier

// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NET_RTNL_MESSAGE_H_
#define SHILL_NET_RTNL_MESSAGE_H_

#include <string>
#include <unordered_map>
#include <vector>

#include <base/macros.h>
#include <base/optional.h>
#include <base/stl_util.h>

#include "shill/net/byte_string.h"
#include "shill/net/ip_address.h"
#include "shill/net/shill_export.h"

struct rtattr;

namespace shill {

struct RTNLHeader;

using RTNLAttrMap = std::unordered_map<uint16_t, ByteString>;

class SHILL_EXPORT RTNLMessage {
 public:
  enum Type {
    kTypeUnknown,
    kTypeLink,
    kTypeAddress,
    kTypeRoute,
    kTypeRule,
    kTypeRdnss,
    kTypeDnssl,
    kTypeNeighbor,
  };

  enum Mode { kModeUnknown, kModeGet, kModeAdd, kModeDelete, kModeQuery };

  struct LinkStatus {
    LinkStatus() : type(0), flags(0), change(0) {}
    LinkStatus(unsigned int in_type,
               unsigned int in_flags,
               unsigned int in_change,
               base::Optional<std::string> kind = base::nullopt)
        : type(in_type), flags(in_flags), change(in_change), kind(kind) {}
    std::string ToString() const;
    unsigned int type;
    unsigned int flags;
    unsigned int change;
    base::Optional<std::string> kind;
  };

  struct AddressStatus {
    AddressStatus() : prefix_len(0), flags(0), scope(0) {}
    AddressStatus(unsigned char prefix_len_in,
                  unsigned char flags_in,
                  unsigned char scope_in)
        : prefix_len(prefix_len_in), flags(flags_in), scope(scope_in) {}
    std::string ToString() const;
    unsigned char prefix_len;
    unsigned char flags;
    unsigned char scope;
  };

  struct RouteStatus {
    RouteStatus()
        : dst_prefix(0),
          src_prefix(0),
          table(0),
          protocol(0),
          scope(0),
          type(0),
          flags(0) {}
    RouteStatus(unsigned char dst_prefix_in,
                unsigned char src_prefix_in,
                unsigned char table_in,
                unsigned char protocol_in,
                unsigned char scope_in,
                unsigned char type_in,
                unsigned flags_in)
        : dst_prefix(dst_prefix_in),
          src_prefix(src_prefix_in),
          table(table_in),
          protocol(protocol_in),
          scope(scope_in),
          type(type_in),
          flags(flags_in) {}
    std::string ToString() const;
    unsigned char dst_prefix;
    unsigned char src_prefix;
    unsigned char table;
    unsigned char protocol;
    unsigned char scope;
    unsigned char type;
    unsigned flags;
  };

  struct NeighborStatus {
    NeighborStatus() : state(0), flags(0), type(0) {}
    NeighborStatus(uint16_t state_in, uint8_t flags_in, uint8_t type_in)
        : state(state_in), flags(flags_in), type(type_in) {}
    std::string ToString() const;
    uint16_t state;
    uint8_t flags;
    uint8_t type;
  };

  struct RdnssOption {
    RdnssOption() : lifetime(0) {}
    RdnssOption(uint32_t lifetime_in, std::vector<IPAddress> addresses_in)
        : lifetime(lifetime_in), addresses(addresses_in) {}
    std::string ToString() const;
    uint32_t lifetime;
    std::vector<IPAddress> addresses;
  };

  // Empty constructor
  RTNLMessage();
  // Build an RTNL message from arguments
  RTNLMessage(Type type,
              Mode mode,
              uint16_t flags,
              uint32_t seq,
              uint32_t pid,
              int32_t interface_index,
              IPAddress::Family family);

  // Parse an RTNL message.  Returns true on success.
  bool Decode(const ByteString& data);
  // Encode an RTNL message.  Returns empty ByteString on failure.
  ByteString Encode() const;
  // Reset all fields.
  void Reset();

  // Getters and setters
  Type type() const { return type_; }
  Mode mode() const { return mode_; }
  uint16_t flags() const { return flags_; }
  uint32_t seq() const { return seq_; }
  void set_seq(uint32_t seq) { seq_ = seq; }
  uint32_t pid() const { return pid_; }
  int32_t interface_index() const { return interface_index_; }
  IPAddress::Family family() const { return family_; }

  static std::string ModeToString(Mode mode);
  std::string ToString() const;

  const LinkStatus& link_status() const { return link_status_; }
  void set_link_status(const LinkStatus& link_status) {
    link_status_ = link_status;
  }
  const AddressStatus& address_status() const { return address_status_; }
  void set_address_status(const AddressStatus& address_status) {
    address_status_ = address_status;
  }
  const RouteStatus& route_status() const { return route_status_; }
  void set_route_status(const RouteStatus& route_status) {
    route_status_ = route_status;
  }
  const RdnssOption& rdnss_option() const { return rdnss_option_; }
  void set_rdnss_option(const RdnssOption& rdnss_option) {
    rdnss_option_ = rdnss_option;
  }
  const NeighborStatus& neighbor_status() const { return neighbor_status_; }
  void set_neighbor_status(const NeighborStatus& neighbor_status) {
    neighbor_status_ = neighbor_status;
  }
  // GLint hates "unsigned short", and I don't blame it, but that's the
  // type that's used in the system headers.  Use uint16_t instead and hope
  // that the conversion never ends up truncating on some strange platform.
  bool HasAttribute(uint16_t attr) const {
    return base::ContainsKey(attributes_, attr);
  }
  const ByteString GetAttribute(uint16_t attr) const {
    return HasAttribute(attr) ? attributes_.find(attr)->second : ByteString(0);
  }
  void SetAttribute(uint16_t attr, const ByteString& val) {
    attributes_[attr] = val;
  }

 private:
  SHILL_PRIVATE bool DecodeInternal(const ByteString& msg);
  SHILL_PRIVATE bool DecodeLink(const RTNLHeader* hdr,
                                Mode mode,
                                rtattr** attr_data,
                                int* attr_length);
  SHILL_PRIVATE bool DecodeAddress(const RTNLHeader* hdr,
                                   Mode mode,
                                   rtattr** attr_data,
                                   int* attr_length);
  SHILL_PRIVATE bool DecodeRoute(const RTNLHeader* hdr,
                                 Mode mode,
                                 rtattr** attr_data,
                                 int* attr_length);
  SHILL_PRIVATE bool DecodeRule(const RTNLHeader* hdr,
                                Mode mode,
                                rtattr** attr_data,
                                int* attr_length);
  SHILL_PRIVATE bool DecodeNdUserOption(const RTNLHeader* hdr,
                                        Mode mode,
                                        rtattr** attr_data,
                                        int* attr_length);
  SHILL_PRIVATE bool ParseRdnssOption(const uint8_t* data,
                                      int length,
                                      uint32_t lifetime);
  SHILL_PRIVATE bool DecodeNeighbor(const RTNLHeader* hdr,
                                    Mode mode,
                                    rtattr** attr_data,
                                    int* attr_length);
  SHILL_PRIVATE bool EncodeLink(RTNLHeader* hdr) const;
  SHILL_PRIVATE bool EncodeAddress(RTNLHeader* hdr) const;
  SHILL_PRIVATE bool EncodeRoute(RTNLHeader* hdr) const;
  SHILL_PRIVATE bool EncodeNeighbor(RTNLHeader* hdr) const;

  Type type_;
  Mode mode_;
  uint16_t flags_;
  uint32_t seq_;
  uint32_t pid_;
  int32_t interface_index_;
  IPAddress::Family family_;
  LinkStatus link_status_;
  AddressStatus address_status_;
  RouteStatus route_status_;
  NeighborStatus neighbor_status_;
  RdnssOption rdnss_option_;
  RTNLAttrMap attributes_;
  // NOTE: Update Reset() accordingly when adding a new member field.

  DISALLOW_COPY_AND_ASSIGN(RTNLMessage);
};

}  // namespace shill

#endif  // SHILL_NET_RTNL_MESSAGE_H_

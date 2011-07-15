// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RTNL_MESSAGE_
#define SHILL_RTNL_MESSAGE_

#include <base/basictypes.h>
#include <base/hash_tables.h>
#include <base/stl_util-inl.h>

#include "shill/byte_string.h"
#include "shill/ip_address.h"

struct rtattr;

namespace shill {

struct RTNLHeader;

class RTNLMessage {
  public:
    enum MessageType {
      kMessageTypeUnknown,
      kMessageTypeLink,
      kMessageTypeAddress,
      kMessageTypeRoute
    };

    enum MessageMode {
      kMessageModeUnknown,
      kMessageModeGet,
      kMessageModeAdd,
      kMessageModeDelete
    };

    struct LinkStatus {
      LinkStatus()
          : type(0),
            flags(0),
            change(0) {}
      LinkStatus(unsigned int in_type,
                 unsigned int in_flags,
                 unsigned int in_change)
          : type(in_type),
            flags(in_flags),
            change(in_change) {}
      unsigned int type;
      unsigned int flags;
      unsigned int change;
    };

    struct AddressStatus {
      AddressStatus()
          : prefix_len(0),
            flags(0),
            scope(0) {}
      AddressStatus(unsigned char prefix_len_in,
                    unsigned char flags_in,
                    unsigned char scope_in)
          : prefix_len(prefix_len_in),
            flags(flags_in),
            scope(scope_in) {}
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
                  unsigned char flags_in)
          : dst_prefix(dst_prefix_in),
            src_prefix(src_prefix_in),
            table(table_in),
            protocol(protocol_in),
            scope(scope_in),
            type(type_in),
            flags(flags_in) {}
      unsigned char dst_prefix;
      unsigned char src_prefix;
      unsigned char table;
      unsigned char protocol;
      unsigned char scope;
      unsigned char type;
      unsigned char flags;
    };

    // Empty constructor
    RTNLMessage();
    // Build an RTNL message from arguments
    RTNLMessage(MessageType type,
                MessageMode mode,
                unsigned int flags,
                uint32 seq,
                uint32 pid,
                int interface_index,
                IPAddress::Family family);

    // Parse an RTNL message.  Returns true on success.
    bool Decode(const ByteString &data);
    // Encode an RTNL message.  Returns empty ByteString on failure.
    ByteString Encode();

    // Getters and setters
    MessageType type() { return type_; }
    MessageMode mode() { return mode_; }
    uint16 flags() { return flags_; }
    uint32 seq() { return seq_; }
    void set_seq(uint32 seq) { seq_ = seq; }
    uint32 pid() { return pid_; }
    uint32 interface_index() { return interface_index_; }
    IPAddress::Family family() { return family_; }

    const LinkStatus &link_status() { return link_status_; }
    void set_link_status(LinkStatus link_status) { link_status_ = link_status; }
    const AddressStatus &address_status() { return address_status_; }
    void set_address_status(AddressStatus address_status) {
      address_status_ = address_status;
    }
    const RouteStatus &route_status() { return route_status_; }
    void set_route_status(RouteStatus route_status) {
      route_status_ = route_status;
    }
    // GLint hates "unsigned short", and I don't blame it, but that's the
    // type that's used in the system headers.  Use uint16 instead and hope
    // that the conversion never ends up truncating on some strange platform.
    bool HasAttribute(uint16 attr) {
      return ContainsKey(attributes_, attr);
    }
    const ByteString GetAttribute(uint16 attr) {
      return HasAttribute(attr) ? attributes_[attr] : ByteString(0);
    }
    void SetAttribute(uint16 attr, const ByteString &val) {
      attributes_[attr] = val;
    }

  private:
    bool DecodeInternal(const ByteString &msg);
    bool DecodeLink(const RTNLHeader *hdr,
                    MessageMode mode,
                    rtattr **attr_data,
                    int *attr_length);
    bool DecodeAddress(const RTNLHeader *hdr,
                       MessageMode mode,
                       rtattr **attr_data,
                       int *attr_length);
    bool DecodeRoute(const RTNLHeader *hdr,
                     MessageMode mode,
                     rtattr **attr_data,
                     int *attr_length);
    void EncodeLink(RTNLHeader *hdr);
    void EncodeAddress(RTNLHeader *hdr);
    void EncodeRoute(RTNLHeader *hdr);

    MessageType type_;
    MessageMode mode_;
    uint16 flags_;
    uint32 seq_;
    uint32 pid_;
    unsigned int interface_index_;
    IPAddress::Family family_;
    LinkStatus link_status_;
    AddressStatus address_status_;
    RouteStatus route_status_;
    base::hash_map<uint16, ByteString> attributes_;

    DISALLOW_COPY_AND_ASSIGN(RTNLMessage);
};

}  // namespace shill

#endif  // SHILL_RTNL_MESSAGE_

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/rtnl_message.h"

#include <base/logging.h>

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

namespace shill {

struct RTNLHeader {
  RTNLHeader() {
    memset(this, 0, sizeof(*this));
  }
  struct nlmsghdr hdr;
  union {
    struct ifinfomsg ifi;
    struct ifaddrmsg ifa;
    struct rtmsg rtm;
    struct rtgenmsg gen;
  };
};

RTNLMessage::RTNLMessage()
    : type_(kTypeUnknown),
      mode_(kModeUnknown),
      flags_(0),
      interface_index_(0),
      family_(IPAddress::kFamilyUnknown) {}

RTNLMessage::RTNLMessage(Type type,
                         Mode mode,
                         unsigned int flags,
                         uint32 seq,
                         uint32 pid,
                         int interface_index,
                         IPAddress::Family family)
    : type_(type),
      mode_(mode),
      flags_(flags),
      seq_(seq),
      pid_(pid),
      interface_index_(interface_index),
      family_(family) {}

bool RTNLMessage::Decode(const ByteString &msg) {
  bool ret = DecodeInternal(msg);
  if (!ret) {
    Reset();
  }
  return ret;
}

bool RTNLMessage::DecodeInternal(const ByteString &msg) {
  const RTNLHeader *hdr =
      reinterpret_cast<const RTNLHeader *>(msg.GetConstData());

  if (msg.GetLength() < sizeof(hdr->hdr) ||
      msg.GetLength() < hdr->hdr.nlmsg_len)
    return false;

  Mode mode = kModeUnknown;
  switch (hdr->hdr.nlmsg_type) {
  case RTM_NEWLINK:
  case RTM_NEWADDR:
  case RTM_NEWROUTE:
    mode = kModeAdd;
    break;

  case RTM_DELLINK:
  case RTM_DELADDR:
  case RTM_DELROUTE:
    mode = kModeDelete;
    break;

  default:
    return false;
  }

  rtattr *attr_data = NULL;
  int attr_length = 0;

  switch (hdr->hdr.nlmsg_type) {
  case RTM_NEWLINK:
  case RTM_DELLINK:
    if (!DecodeLink(hdr, mode, &attr_data, &attr_length))
      return false;
    break;

  case RTM_NEWADDR:
  case RTM_DELADDR:
    if (!DecodeAddress(hdr, mode, &attr_data, &attr_length))
      return false;
    break;

  case RTM_NEWROUTE:
  case RTM_DELROUTE:
    if (!DecodeRoute(hdr, mode, &attr_data, &attr_length))
      return false;
    break;

  default:
    NOTREACHED();
  }

  flags_ = hdr->hdr.nlmsg_flags;
  seq_ = hdr->hdr.nlmsg_seq;
  pid_ = hdr->hdr.nlmsg_pid;

  while (attr_data && RTA_OK(attr_data, attr_length)) {
    SetAttribute(
        attr_data->rta_type,
        ByteString(reinterpret_cast<unsigned char *>(RTA_DATA(attr_data)),
                   RTA_PAYLOAD(attr_data)));
    attr_data = RTA_NEXT(attr_data, attr_length);
  }

  if (attr_length) {
    // We hit a parse error while going through the attributes
    attributes_.clear();
    return false;
  }

  return true;
}

bool RTNLMessage::DecodeLink(const RTNLHeader *hdr,
                             Mode mode,
                             rtattr **attr_data,
                             int *attr_length) {
  if (hdr->hdr.nlmsg_len < NLMSG_LENGTH(sizeof(hdr->ifi))) {
    return false;
  }

  mode_ = mode;
  *attr_data = IFLA_RTA(NLMSG_DATA(&hdr->hdr));
  *attr_length = IFLA_PAYLOAD(&hdr->hdr);

  type_ = kTypeLink;
  family_ = hdr->ifi.ifi_family;
  interface_index_ = hdr->ifi.ifi_index;
  set_link_status(LinkStatus(hdr->ifi.ifi_type,
                             hdr->ifi.ifi_flags,
                             hdr->ifi.ifi_change));
  return true;
}

bool RTNLMessage::DecodeAddress(const RTNLHeader *hdr,
                                Mode mode,
                                rtattr **attr_data,
                                int *attr_length) {
  if (hdr->hdr.nlmsg_len < NLMSG_LENGTH(sizeof(hdr->ifa))) {
    return false;
  }
  mode_ = mode;
  *attr_data = IFA_RTA(NLMSG_DATA(&hdr->hdr));
  *attr_length = IFA_PAYLOAD(&hdr->hdr);

  type_ = kTypeAddress;
  family_ = hdr->ifa.ifa_family;
  interface_index_ = hdr->ifa.ifa_index;
  set_address_status(AddressStatus(hdr->ifa.ifa_prefixlen,
                                   hdr->ifa.ifa_flags,
                                   hdr->ifa.ifa_scope));
  return true;
}

bool RTNLMessage::DecodeRoute(const RTNLHeader *hdr,
                              Mode mode,
                              rtattr **attr_data,
                              int *attr_length) {
  if (hdr->hdr.nlmsg_len < NLMSG_LENGTH(sizeof(hdr->rtm))) {
    return false;
  }
  mode_ = mode;
  *attr_data = RTM_RTA(NLMSG_DATA(&hdr->hdr));
  *attr_length = RTM_PAYLOAD(&hdr->hdr);

  type_ = kTypeRoute;
  family_ = hdr->rtm.rtm_family;
  set_route_status(RouteStatus(hdr->rtm.rtm_dst_len,
                               hdr->rtm.rtm_src_len,
                               hdr->rtm.rtm_table,
                               hdr->rtm.rtm_protocol,
                               hdr->rtm.rtm_scope,
                               hdr->rtm.rtm_type,
                               hdr->rtm.rtm_flags));
  return true;
}

ByteString RTNLMessage::Encode() const {
  if (type_ != kTypeLink &&
      type_ != kTypeAddress &&
      type_ != kTypeRoute) {
    return ByteString();
  }

  RTNLHeader hdr;
  hdr.hdr.nlmsg_flags = flags_;
  hdr.hdr.nlmsg_seq = seq_;
  hdr.hdr.nlmsg_pid = pid_;

  if (mode_ == kModeGet) {
    if (type_ == kTypeLink) {
      hdr.hdr.nlmsg_type = RTM_GETLINK;
    } else if (type_ == kTypeAddress) {
      hdr.hdr.nlmsg_type = RTM_GETADDR;
    } else if (type_ == kTypeRoute) {
      hdr.hdr.nlmsg_type = RTM_GETROUTE;
    } else {
      NOTIMPLEMENTED();
      return ByteString();
    }
    hdr.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(hdr.gen));
    hdr.hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
    hdr.gen.rtgen_family = family_;
  } else {
    switch (type_) {
    case kTypeLink:
      if (!EncodeLink(&hdr)) {
        return ByteString();
      }
      break;

    case kTypeAddress:
      if (!EncodeAddress(&hdr)) {
        return ByteString();
      }
      break;

    case kTypeRoute:
      if (!EncodeRoute(&hdr)) {
        return ByteString();
      }
      break;

    default:
      NOTREACHED();
    }
  }

  size_t header_length = hdr.hdr.nlmsg_len;
  ByteString attributes;

  base::hash_map<uint16, ByteString>::const_iterator attr;
  for (attr = attributes_.begin(); attr != attributes_.end(); ++attr) {
    size_t len = RTA_LENGTH(attr->second.GetLength());
    hdr.hdr.nlmsg_len = NLMSG_ALIGN(hdr.hdr.nlmsg_len) + RTA_ALIGN(len);

    struct rtattr rt_attr = { len, attr->first };
    ByteString attr_header(reinterpret_cast<unsigned char *>(&rt_attr),
                           sizeof(rt_attr));
    attr_header.Resize(RTA_ALIGN(attr_header.GetLength()));
    attributes.Append(attr_header);

    ByteString attr_data(attr->second);
    attr_data.Resize(RTA_ALIGN(attr_data.GetLength()));
    attributes.Append(attr_data);
  }

  ByteString packet(reinterpret_cast<unsigned char *>(&hdr), header_length);
  packet.Append(attributes);

  return packet;
}

bool RTNLMessage::EncodeLink(RTNLHeader *hdr) const {
  switch (mode_) {
    case kModeAdd:
      hdr->hdr.nlmsg_type = RTM_NEWLINK;
      break;
    case kModeDelete:
      hdr->hdr.nlmsg_type = RTM_DELLINK;
      break;
    case kModeQuery:
      hdr->hdr.nlmsg_type = RTM_GETLINK;
      break;
    default:
      NOTIMPLEMENTED();
      return false;
  }
  hdr->hdr.nlmsg_len = NLMSG_LENGTH(sizeof(hdr->ifi));
  hdr->ifi.ifi_family = family_;
  hdr->ifi.ifi_index = interface_index_;
  hdr->ifi.ifi_type = link_status_.type;
  hdr->ifi.ifi_flags = link_status_.flags;
  hdr->ifi.ifi_change = link_status_.change;
  return true;
}

bool RTNLMessage::EncodeAddress(RTNLHeader *hdr) const {
  switch (mode_) {
    case kModeAdd:
      hdr->hdr.nlmsg_type = RTM_NEWADDR;
      break;
    case kModeDelete:
      hdr->hdr.nlmsg_type = RTM_DELADDR;
      break;
    case kModeQuery:
      hdr->hdr.nlmsg_type = RTM_GETADDR;
      break;
    default:
      NOTIMPLEMENTED();
      return false;
  }
  hdr->hdr.nlmsg_len = NLMSG_LENGTH(sizeof(hdr->ifa));
  hdr->ifa.ifa_family = family_;
  hdr->ifa.ifa_prefixlen = address_status_.prefix_len;
  hdr->ifa.ifa_flags = address_status_.flags;
  hdr->ifa.ifa_scope = address_status_.scope;
  hdr->ifa.ifa_index = interface_index_;
  return true;
}

bool RTNLMessage::EncodeRoute(RTNLHeader *hdr) const {
  switch (mode_) {
    case kModeAdd:
      hdr->hdr.nlmsg_type = RTM_NEWROUTE;
      break;
    case kModeDelete:
      hdr->hdr.nlmsg_type = RTM_DELROUTE;
      break;
    case kModeQuery:
      hdr->hdr.nlmsg_type = RTM_GETROUTE;
      break;
    default:
      NOTIMPLEMENTED();
      return false;
  }
  hdr->hdr.nlmsg_len = NLMSG_LENGTH(sizeof(hdr->rtm));
  hdr->rtm.rtm_family = family_;
  hdr->rtm.rtm_dst_len = route_status_.dst_prefix;
  hdr->rtm.rtm_src_len = route_status_.src_prefix;
  hdr->rtm.rtm_table = route_status_.table;
  hdr->rtm.rtm_protocol = route_status_.protocol;
  hdr->rtm.rtm_scope = route_status_.scope;
  hdr->rtm.rtm_type = route_status_.type;
  hdr->rtm.rtm_flags = route_status_.flags;
  return true;
}

void RTNLMessage::Reset() {
  mode_ = kModeUnknown;
  type_ = kTypeUnknown;
  flags_ = 0;
  seq_ = 0;
  interface_index_ = 0;
  family_ = IPAddress::kFamilyUnknown;
  link_status_ = LinkStatus();
  address_status_ = AddressStatus();
  route_status_ = RouteStatus();
  attributes_.clear();
}

}  // namespace shill

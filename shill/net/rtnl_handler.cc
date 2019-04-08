// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/rtnl_handler.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/ether.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <limits>

#include <base/bind.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>

#include "shill/logging.h"
#include "shill/net/io_handler.h"
#include "shill/net/ip_address.h"
#include "shill/net/ndisc.h"
#include "shill/net/netlink_fd.h"
#include "shill/net/rtnl_listener.h"
#include "shill/net/rtnl_message.h"
#include "shill/net/sockets.h"

using base::Bind;
using base::Unretained;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kRTNL;
static std::string ObjectID(const RTNLHandler* obj) {
  return "(rtnl_handler)";
}
}  // namespace Logging

const uint32_t RTNLHandler::kRequestLink = 1;
const uint32_t RTNLHandler::kRequestAddr = 2;
const uint32_t RTNLHandler::kRequestRoute = 4;
const uint32_t RTNLHandler::kRequestRule = 8;
const uint32_t RTNLHandler::kRequestRdnss = 16;
const uint32_t RTNLHandler::kRequestNeighbor = 32;
const uint32_t RTNLHandler::kRequestBridgeNeighbor = 64;
const int RTNLHandler::kErrorWindowSize = 16;

namespace {
base::LazyInstance<RTNLHandler>::DestructorAtExit g_rtnl_handler =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

RTNLHandler::RTNLHandler()
    : sockets_(new Sockets()),
      in_request_(false),
      rtnl_socket_(Sockets::kInvalidFileDescriptor),
      request_flags_(0),
      request_sequence_(0),
      last_dump_sequence_(0),
      io_handler_factory_(
          IOHandlerFactoryContainer::GetInstance()->GetIOHandlerFactory()) {
  error_mask_window_.resize(kErrorWindowSize);
  SLOG(this, 2) << "RTNLHandler created";
}

RTNLHandler::~RTNLHandler() {
  SLOG(this, 2) << "RTNLHandler removed";
  Stop();
}

RTNLHandler* RTNLHandler::GetInstance() {
  return g_rtnl_handler.Pointer();
}

void RTNLHandler::Start(uint32_t netlink_groups_mask) {
  if (rtnl_socket_ != Sockets::kInvalidFileDescriptor)
    return;

  rtnl_socket_ = OpenNetlinkSocketFD(
      sockets_.get(), NETLINK_ROUTE, netlink_groups_mask);
  if (rtnl_socket_ < 0) {
    LOG(ERROR) << "Failed to open rtnl socket";
    return;
  }

  rtnl_handler_.reset(io_handler_factory_->CreateIOInputHandler(
      rtnl_socket_,
      Bind(&RTNLHandler::ParseRTNL, Unretained(this)),
      Bind(&RTNLHandler::OnReadError, Unretained(this))));

  NextRequest(last_dump_sequence_);
  SLOG(this, 2) << "RTNLHandler started";
}

void RTNLHandler::Stop() {
  rtnl_handler_.reset();
  // Close the socket if it is currently open.
  if (rtnl_socket_ != Sockets::kInvalidFileDescriptor) {
    sockets_->Close(rtnl_socket_);
    rtnl_socket_ = Sockets::kInvalidFileDescriptor;
  }
  in_request_ = false;
  request_flags_ = 0;
  SLOG(this, 2) << "RTNLHandler stopped";
}

void RTNLHandler::AddListener(RTNLListener* to_add) {
  for (const auto& listener : listeners_) {
    if (to_add == listener)
      return;
  }
  listeners_.push_back(to_add);
  SLOG(this, 2) << "RTNLHandler added listener";
}

void RTNLHandler::RemoveListener(RTNLListener* to_remove) {
  for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
    if (to_remove == *it) {
      listeners_.erase(it);
      return;
    }
  }
  SLOG(this, 2) << "RTNLHandler removed listener";
}

void RTNLHandler::SetInterfaceFlags(int interface_index, unsigned int flags,
                                    unsigned int change) {
  if (rtnl_socket_ == Sockets::kInvalidFileDescriptor) {
    LOG(ERROR) << __func__ << " called while not started.  "
        "Assuming we are in unit tests.";
    return;
  }

  RTNLMessage msg(
      RTNLMessage::kTypeLink,
      RTNLMessage::kModeAdd,
      NLM_F_REQUEST,
      0,  // sequence to be filled in by RTNLHandler::SendMessage().
      0,  // pid.
      interface_index,
      IPAddress::kFamilyUnknown);

  msg.set_link_status(RTNLMessage::LinkStatus(ARPHRD_VOID, flags, change));

  ErrorMask error_mask;
  if ((flags & IFF_UP) == 0) {
    error_mask.insert(ENODEV);
  }

  SendMessageWithErrorMask(&msg, error_mask);
}

void RTNLHandler::SetInterfaceMTU(int interface_index, unsigned int mtu) {
  RTNLMessage msg(
      RTNLMessage::kTypeLink,
      RTNLMessage::kModeAdd,
      NLM_F_REQUEST,
      0,  // sequence to be filled in by RTNLHandler::SendMessage().
      0,  // pid.
      interface_index,
      IPAddress::kFamilyUnknown);

  msg.SetAttribute(
      IFLA_MTU,
      ByteString(reinterpret_cast<unsigned char*>(&mtu), sizeof(mtu)));

  CHECK(SendMessage(&msg));
}

void RTNLHandler::SetInterfaceMac(int interface_index,
                                  const ByteString& mac_address) {
  RTNLMessage msg(RTNLMessage::kTypeLink,
                  RTNLMessage::kModeAdd,
                  NLM_F_REQUEST,
                  0,  // sequence to be filled in by RTNLHandler::SendMessage().
                  0,  // pid.
                  interface_index,
                  IPAddress::kFamilyUnknown);

  msg.SetAttribute(IFLA_ADDRESS, mac_address);

  CHECK(SendMessage(&msg));
}

void RTNLHandler::RequestDump(uint32_t request_flags) {
  if (rtnl_socket_ == Sockets::kInvalidFileDescriptor) {
    LOG(ERROR) << __func__ << " called while not started.  "
        "Assuming we are in unit tests.";
    return;
  }

  request_flags_ |= request_flags;

  SLOG(this, 2) << base::StringPrintf("RTNLHandler got request to dump 0x%x",
                                      request_flags);

  if (!in_request_) {
    NextRequest(last_dump_sequence_);
  }
}

void RTNLHandler::DispatchEvent(int type, const RTNLMessage& msg) {
  for (const auto& listener : listeners_) {
    listener->NotifyEvent(type, msg);
  }
}

void RTNLHandler::NextRequest(uint32_t seq) {
  uint32_t flag = 0;
  RTNLMessage::Type type;

  SLOG(this, 2) << base::StringPrintf("RTNLHandler nextrequest %d %d 0x%x", seq,
                                      last_dump_sequence_, request_flags_);

  if (seq != last_dump_sequence_)
    return;

  IPAddress::Family family = IPAddress::kFamilyUnknown;
  if ((request_flags_ & kRequestAddr) != 0) {
    type = RTNLMessage::kTypeAddress;
    flag = kRequestAddr;
  } else if ((request_flags_ & kRequestRoute) != 0) {
    type = RTNLMessage::kTypeRoute;
    flag = kRequestRoute;
  } else if ((request_flags_ & kRequestRule) != 0) {
    type = RTNLMessage::kTypeRule;
    flag = kRequestRule;
  } else if ((request_flags_ & kRequestLink) != 0) {
    type = RTNLMessage::kTypeLink;
    flag = kRequestLink;
  } else if ((request_flags_ & kRequestNeighbor) != 0) {
    type = RTNLMessage::kTypeNeighbor;
    flag = kRequestNeighbor;
  } else if ((request_flags_ & kRequestBridgeNeighbor) != 0) {
    type = RTNLMessage::kTypeNeighbor;
    flag = kRequestBridgeNeighbor;
    family = AF_BRIDGE;
  } else {
    SLOG(this, 2) << "Done with requests";
    in_request_ = false;
    return;
  }

  RTNLMessage msg(
      type,
      RTNLMessage::kModeGet,
      0,
      0,
      0,
      0,
      family);
  CHECK(SendMessage(&msg));

  last_dump_sequence_ = msg.seq();
  request_flags_ &= ~flag;
  in_request_ = true;
}

void RTNLHandler::ParseRTNL(InputData* data) {
  const unsigned char* buf = data->buf;
  const unsigned char* end = buf + data->len;

  while (buf < end) {
    const struct nlmsghdr* hdr = reinterpret_cast<const struct nlmsghdr*>(buf);
    if (!NLMSG_OK(hdr, static_cast<unsigned int>(end - buf)))
      break;

    SLOG(this, 5) << __func__ << ": received payload (" << end - buf << ")";

    RTNLMessage msg;
    ByteString payload(reinterpret_cast<const unsigned char*>(hdr),
                       hdr->nlmsg_len);
    SLOG(this, 5) << "RTNL received payload length " << payload.GetLength()
                  << ": \"" << payload.HexEncode() << "\"";
    if (!msg.Decode(payload)) {
      SLOG(this, 5) << __func__ << ": rtnl packet type " << hdr->nlmsg_type
                    << " length " << hdr->nlmsg_len << " sequence "
                    << hdr->nlmsg_seq;

      switch (hdr->nlmsg_type) {
        case NLMSG_NOOP:
        case NLMSG_OVERRUN:
          break;
        case NLMSG_DONE:
          GetAndClearErrorMask(hdr->nlmsg_seq);  // Clear any queued error mask.
          NextRequest(hdr->nlmsg_seq);
          break;
        case NLMSG_ERROR:
          {
            if (hdr->nlmsg_len < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
              SLOG(this, 5) << "invalid error message header: length "
                            << hdr->nlmsg_len;
              break;
            }
            struct nlmsgerr* err =
                reinterpret_cast<nlmsgerr*>(NLMSG_DATA(hdr));
            if (err->error >= 0 ||
                err->error == std::numeric_limits<int>::min()) {
              LOG(ERROR) << "sequence " << hdr->nlmsg_seq
                         << " received unexpected error code " << err->error;
            } else {
              int error_number = -err->error;
              std::ostringstream message;
              message << "sequence " << hdr->nlmsg_seq << " received error "
                << error_number << " ("
                << strerror(error_number) << ")";
              if (!base::ContainsKey(GetAndClearErrorMask(hdr->nlmsg_seq),
                    error_number)) {
                LOG(ERROR) << message.str();
              } else {
                SLOG(this, 3) << message.str();
              }
            }
            break;
          }
        default:
          NOTIMPLEMENTED() << "Unknown NL message type.";
      }
    } else {
      switch (msg.type()) {
        case RTNLMessage::kTypeLink:
          DispatchEvent(kRequestLink, msg);
          break;
        case RTNLMessage::kTypeAddress:
          DispatchEvent(kRequestAddr, msg);
          break;
        case RTNLMessage::kTypeRoute:
          DispatchEvent(kRequestRoute, msg);
          break;
        case RTNLMessage::kTypeRule:
          DispatchEvent(kRequestRule, msg);
          break;
        case RTNLMessage::kTypeRdnss:
          DispatchEvent(kRequestRdnss, msg);
          break;
        case RTNLMessage::kTypeNeighbor:
          DispatchEvent(kRequestNeighbor, msg);
          break;
        case RTNLMessage::kTypeDnssl:
          NOTIMPLEMENTED();
          break;
        default:
          NOTIMPLEMENTED() << "Unknown RTNL message type.";
      }
    }
    buf += NLMSG_ALIGN(hdr->nlmsg_len);
  }
}

bool RTNLHandler::AddressRequest(int interface_index,
                                 RTNLMessage::Mode mode,
                                 int flags,
                                 const IPAddress& local,
                                 const IPAddress& broadcast,
                                 const IPAddress& peer) {
  CHECK(local.family() == broadcast.family());
  CHECK(local.family() == peer.family());

  RTNLMessage msg(
      RTNLMessage::kTypeAddress,
      mode,
      NLM_F_REQUEST | flags,
      0,
      0,
      interface_index,
      local.family());

  msg.set_address_status(RTNLMessage::AddressStatus(
      local.prefix(),
      0,
      0));

  msg.SetAttribute(IFA_LOCAL, local.address());
  if (!broadcast.IsDefault()) {
    msg.SetAttribute(IFA_BROADCAST, broadcast.address());
  }
  if (!peer.IsDefault()) {
    msg.SetAttribute(IFA_ADDRESS, peer.address());
  }

  return SendMessage(&msg);
}

bool RTNLHandler::AddInterfaceAddress(int interface_index,
                                      const IPAddress& local,
                                      const IPAddress& broadcast,
                                      const IPAddress& peer) {
    return AddressRequest(interface_index,
                          RTNLMessage::kModeAdd,
                          NLM_F_CREATE | NLM_F_EXCL | NLM_F_ECHO,
                          local,
                          broadcast,
                          peer);
}

bool RTNLHandler::RemoveInterfaceAddress(int interface_index,
                                         const IPAddress& local) {
  return AddressRequest(interface_index,
                        RTNLMessage::kModeDelete,
                        NLM_F_ECHO,
                        local,
                        IPAddress(local.family()),
                        IPAddress(local.family()));
}

bool RTNLHandler::RemoveInterface(int interface_index) {
  RTNLMessage msg(
      RTNLMessage::kTypeLink,
      RTNLMessage::kModeDelete,
      NLM_F_REQUEST,
      0,
      0,
      interface_index,
      IPAddress::kFamilyUnknown);
  return SendMessage(&msg);
}

int RTNLHandler::GetInterfaceIndex(const string& interface_name) {
  if (interface_name.empty()) {
    LOG(ERROR) << "Empty interface name -- unable to obtain index.";
    return -1;
  }
  struct ifreq ifr;
  if (interface_name.size() >= sizeof(ifr.ifr_name)) {
    LOG(ERROR) << "Interface name too long: " << interface_name.size() << " >= "
               << sizeof(ifr.ifr_name);
    return -1;
  }
  int socket = sockets_->Socket(PF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (socket < 0) {
    PLOG(ERROR) << "Unable to open INET socket";
    return -1;
  }
  ScopedSocketCloser socket_closer(sockets_.get(), socket);
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name, interface_name.c_str(), sizeof(ifr.ifr_name));
  if (sockets_->Ioctl(socket, SIOCGIFINDEX, &ifr) < 0) {
    PLOG(ERROR) << "SIOCGIFINDEX error for " << interface_name;
    return -1;
  }
  return ifr.ifr_ifindex;
}

bool RTNLHandler::SendMessageWithErrorMask(RTNLMessage* message,
                                           const ErrorMask& error_mask) {
  SLOG(this, 5) << __func__ << " sequence " << request_sequence_
                << " message type " << message->type() << " mode "
                << message->mode() << " with error mask size "
                << error_mask.size();

  SetErrorMask(request_sequence_, error_mask);
  message->set_seq(request_sequence_);
  ByteString msgdata = message->Encode();

  if (msgdata.GetLength() == 0) {
    return false;
  }

  SLOG(this, 5) << "RTNL sending payload with request sequence "
                << request_sequence_ << ", length " << msgdata.GetLength()
                << ": \"" << msgdata.HexEncode() << "\"";

  request_sequence_++;

  if (sockets_->Send(rtnl_socket_,
                     msgdata.GetConstData(),
                     msgdata.GetLength(),
                     0) < 0) {
    PLOG(ERROR) << "RTNL send failed";
    return false;
  }

  return true;
}

bool RTNLHandler::SendMessage(RTNLMessage* message) {
  ErrorMask error_mask;
  if (message->mode() == RTNLMessage::kModeAdd) {
    error_mask = { EEXIST };
  } else if (message->mode() == RTNLMessage::kModeDelete) {
    error_mask = { ESRCH, ENODEV };
    if (message->type() == RTNLMessage::kTypeAddress) {
      error_mask.insert(EADDRNOTAVAIL);
    }
  }
  return SendMessageWithErrorMask(message, error_mask);
}

bool RTNLHandler::IsSequenceInErrorMaskWindow(uint32_t sequence) {
  return (request_sequence_ - sequence) < kErrorWindowSize;
}

void RTNLHandler::SetErrorMask(uint32_t sequence, const ErrorMask& error_mask) {
  if (IsSequenceInErrorMaskWindow(sequence)) {
    error_mask_window_[sequence % kErrorWindowSize] = error_mask;
  }
}

RTNLHandler::ErrorMask RTNLHandler::GetAndClearErrorMask(uint32_t sequence) {
  ErrorMask error_mask;
  if (IsSequenceInErrorMaskWindow(sequence)) {
    error_mask.swap(error_mask_window_[sequence % kErrorWindowSize]);
  }
  return error_mask;
}

void RTNLHandler::OnReadError(const string& error_msg) {
  LOG(FATAL) << "RTNL Socket read returns error: "
             << error_msg;
}

}  // namespace shill

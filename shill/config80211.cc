// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/config80211.h"

#include <ctype.h>

#include <netlink/msg.h>

#include <map>
#include <sstream>
#include <string>

#include <base/memory/weak_ptr.h>
#include <base/stl_util.h>
#include <base/stringprintf.h>

#include "shill/io_handler.h"
#include "shill/logging.h"
#include "shill/nl80211_socket.h"
#include "shill/scope_logger.h"
#include "shill/user_bound_nlmessage.h"

using base::Bind;
using base::LazyInstance;
using base::StringAppendF;
using std::string;

namespace shill {

namespace {
LazyInstance<Config80211> g_config80211 = LAZY_INSTANCE_INITIALIZER;
}  // namespace

Config80211::EventTypeStrings *Config80211::event_types_ = NULL;

Config80211::Config80211()
    : wifi_state_(kWifiDown),
      dispatcher_(NULL),
      weak_ptr_factory_(this),
      dispatcher_callback_(Bind(&Config80211::HandleIncomingEvents,
                              weak_ptr_factory_.GetWeakPtr())),
      sock_(NULL) {
}

Config80211::~Config80211() {
  // Since Config80211 is a singleton, it should be safe to delete a static
  // member.
  delete event_types_;
  event_types_ = NULL;
}

Config80211 *Config80211::GetInstance() {
  return g_config80211.Pointer();
}

void Config80211::Reset() {
  wifi_state_ = kWifiDown;
  subscribed_events_.clear();
}

bool Config80211::Init(EventDispatcher *dispatcher) {
  if (!sock_) {
    sock_ = new Nl80211Socket;
    if (!sock_) {
      LOG(ERROR) << "No memory";
      return false;
    }

    if (!sock_->Init()) {
      return false;
    }
  }

  if (!event_types_) {
    event_types_ = new EventTypeStrings;
    (*event_types_)[Config80211::kEventTypeConfig] = "config";
    (*event_types_)[Config80211::kEventTypeScan] = "scan";
    (*event_types_)[Config80211::kEventTypeRegulatory] = "regulatory";
    (*event_types_)[Config80211::kEventTypeMlme] = "mlme";
  }

  // Install ourselves in the shill mainloop so we receive messages on the
  // nl80211 socket.
  dispatcher_ = dispatcher;
  if (dispatcher_) {
    dispatcher_handler_.reset(
      dispatcher_->CreateReadyHandler(GetFd(),
                                      IOHandler::kModeInput,
                                      dispatcher_callback_));
  }
  return true;
}

// static
bool Config80211::GetEventTypeString(EventType type, string *value) {
  if (!value) {
    LOG(ERROR) << "NULL |value|";
    return false;
  }
  if (!event_types_) {
    LOG(ERROR) << "NULL |event_types_|";
    return false;
  }

  EventTypeStrings::iterator match = (*event_types_).find(type);
  if (match == (*event_types_).end()) {
    LOG(ERROR) << "Event type " << type << " not found";
    return false;
  }
  *value = match->second;
  return true;
}

void Config80211::SetWifiState(WifiState new_state) {
  if (wifi_state_ == new_state) {
    return;
  }

  // If we're newly-up, subscribe to all the event types that have been
  // requested.
  if (new_state == kWifiUp) {
    SubscribedEvents::const_iterator i;
    for (i = subscribed_events_.begin(); i != subscribed_events_.end(); ++i) {
      string event_type_string;
      GetEventTypeString(*i, &event_type_string);
      ActuallySubscribeToEvents(*i);
    }
  }
  wifi_state_ = new_state;
}

bool Config80211::SubscribeToEvents(EventType type) {
  string event_type_string;
  GetEventTypeString(type, &event_type_string);
  bool it_worked = true;
  if (!ContainsKey(subscribed_events_, type)) {
    if (wifi_state_ == kWifiUp) {
      it_worked = ActuallySubscribeToEvents(type);
    }
    // |subscribed_events_| is a list of events to which we want to subscribe
    // when wifi comes up (including when it comes _back_ up after it goes
    // down sometime in the future).
    subscribed_events_.insert(type);
  }
  return it_worked;
}

bool Config80211::ActuallySubscribeToEvents(EventType type) {
  string group_name;
  string event_type_string;
  GetEventTypeString(type, &event_type_string);

  if (!GetEventTypeString(type, &group_name)) {
    return false;
  }
  if (!sock_->AddGroupMembership(group_name)) {
    return false;
  }
  // No sequence checking for multicast messages.
  if (!sock_->DisableSequenceChecking()) {
    return false;
  }

  // Install the global NetLink Callback for messages along with a parameter.
  // The Netlink Callback's parameter is passed to 'C' as a 'void *'.
  if (!sock_->SetNetlinkCallback(OnNlMessageReceived,
                                static_cast<void *>(
                                    const_cast<Config80211::Callback *>(
                                        &default_callback_)))) {
    return false;
  }
  return true;
}

void Config80211::HandleIncomingEvents(int unused_fd) {
  sock_->GetMessages();
}

// NOTE: the "struct nl_msg" declaration, below, is a complete fabrication
// (but one consistent with the nl_socket_modify_cb call to which
// |OnNlMessageReceived| is a parameter).  |raw_message| is actually a
// "struct sk_buff" but that data type is only visible in the kernel.  We'll
// scrub this erroneous datatype with the "nlmsg_hdr" call, below, which
// extracts an nlmsghdr pointer from |raw_message|.
int Config80211::OnNlMessageReceived(struct nl_msg *raw_message,
                                     void *user_callback_object) {
  SLOG(WiFi, 6) << "\t  Entering " << __func__
                << "( msg:" << raw_message
                << ", cb:" << user_callback_object << ")";

  string output("@");

  nlmsghdr *msg = nlmsg_hdr(raw_message);

  scoped_ptr<UserBoundNlMessage> message(
      UserBoundNlMessageFactory::CreateMessage(msg));
  if (message == NULL) {
    output.append("unknown event");
  } else {
    if (user_callback_object) {
      Config80211::Callback *bound_object =
          static_cast<Config80211::Callback *> (user_callback_object);
      if (!bound_object->is_null()) {
        bound_object->Run(*message);
      }
    }
    StringAppendF(&output, "%s", message->ToString().c_str());
  }

  SLOG(WiFi, 5) << output;

  return NL_SKIP;  // Skip current message, continue parsing buffer.
}

}  // namespace shill.

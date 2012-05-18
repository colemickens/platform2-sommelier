// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This library provides an abstracted interface to the cfg80211 kernel module
// and mac80211 drivers.  These are accessed via a netlink socket using the
// following software stack:
//
//    [shill]
//       |
// [nl80211 library]
//       |
// [libnl_genl/libnl libraries]
//       |
//   (netlink socket)
//       |
// [cfg80211 kernel module]
//       |
// [mac80211 drivers]
//
// Messages go from user-space to kernel-space (i.e., Kernel-Bound) or in the
// other direction (i.e., User-Bound).
//
// For the love of Pete, there are a lot of different types of callbacks,
// here.  I'll try to differentiate:
//
// Config80211 Callback -
//    This is a base::Callback object installed by the user and called by
//    Config80211 for each message it receives.  More specifically, when the
//    user calls Config80211::SubscribeToEvents, Config80211 installs
//    OnNlMessageReceived as a netlink callback function (described below).
//    OnNlMessageReceived, in turn, parses the message from cfg80211 and calls
//    Config80211::Callback with the resultant UserBoundNlMessage.
//
// Netlink Callback -
//    Netlink callbacks are mechanisms installed by the user (well, by
//    Config80211 -- none of these are intended for use by users of
//    Config80211) for the libnl layer to communicate back to the user.  Some
//    callbacks are installed for global use (i.e., the default callback used
//    for all messages) or as an override for a specific message.  Netlink
//    callbacks come in three levels.
//
//    The lowest level (nl_recvmsg_msg_cb_t) is a function installed by
//    Config80211.  These are called by libnl when messages are received from
//    the kernel.
//
//    The medium level (nl_cb) is also used by Config80211.  This, the 'netlink
//    callback structure', encapsualtes a number of netlink callback functions
//    (nl_recvmsg_msg_cb_t, one each for different types of messages).
//
//    The highest level is the NetlinkSocket::Callback object.
//
// Dispatcher Callback -
//    This base::Callback is a private method of Config80211 created and
//    installed behind the scenes.  This is not the callback you're looking
//    for; move along.  This is called by shill's EventDispatcher when there's
//    data waiting for user space code on the netlink socket.  This callback
//    then calls NetlinkSocket::GetMessages which calls nl_recvmsgs_default
//    which, in turn, calls the installed netlink callback function.

#ifndef SHILL_CONFIG80211_H_
#define SHILL_CONFIG80211_H_

#include <iomanip>
#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/lazy_instance.h>

#include "shill/event_dispatcher.h"
#include "shill/io_handler.h"
#include "shill/nl80211_socket.h"

namespace shill {

class KernelBoundNlMessage;
class UserBoundNlMessage;

// Provides a transport-independent ability to receive status from the wifi
// configuration.  In its current implementation, it uses the netlink socket
// interface to interface with the wifi system.
//
// Config80211 is a singleton and, as such, coordinates access to libnl.
class Config80211 {
 public:
  typedef base::Callback<void(const UserBoundNlMessage &)> Callback;

  // The different kinds of events to which we can subscribe (and receive)
  // from cfg80211.
  enum EventType {
    kEventTypeConfig,
    kEventTypeScan,
    kEventTypeRegulatory,
    kEventTypeMlme,
    kEventTypeCount
  };

  virtual ~Config80211();

  // This is a singleton -- use Config80211::GetInstance()->Foo()
  static Config80211 *GetInstance();

  // Performs non-trivial object initialization of the Config80211 singleton.
  bool Init(EventDispatcher *dispatcher);

  // Returns the file descriptor of socket used to read wifi data.
  int GetFd() const { return (sock_ ? sock_->GetFd() : -1); }

  // Install default Config80211 Callback.  The callback is a user-supplied
  // object to be called by the system for user-bound messages that do not
  // have a corresponding messaage-specific callback.  |SetDefaultCallback|
  // should be called before |SubscribeToEvents| since the result of this call
  // are used for that call.
  void SetDefaultCallback(const Callback &callback) {
    default_callback_ = callback; }

  // Uninstall default Config80211 Callback.
  void UnsetDefaultCallback() { default_callback_.Reset(); }

  // TODO(wdg): Add 'SendMessage(KernelBoundNlMessage *message,
  //                             Config80211::Callback *callback);
  // Config80211 needs to handle out-of-order responses using a
  // map <sequence_number, callback> to match callback with message.

  // Return a string corresponding to the passed-in EventType.
  static bool GetEventTypeString(EventType type, std::string *value);

  // Sign-up to receive and log multicast events of a specific type.
  bool SubscribeToEvents(EventType type);

 protected:
  friend struct base::DefaultLazyInstanceTraits<Config80211>;

  explicit Config80211();

 private:
  friend class Config80211Test;

  // EventDispatcher calls this when data is available on our socket.  This
  // callback reads data from the driver, parses that data, and logs it.
  void HandleIncomingEvents(int fd);

  // This is a Netlink Callback.  libnl-80211 calls this method when it
  // receives data from cfg80211.  This method parses those messages and logs
  // them.
  static int OnNlMessageReceived(struct nl_msg *msg, void *arg);

  // Config80211 Callback, OnNlMessageReceived invokes this User-supplied
  // callback object when _it_ gets called to read libnl data.
  Callback default_callback_;

  // TODO(wdg): implement the following.
  // std::map<uint32_t, Callback> message_callback_;

  static std::map<EventType, std::string> *event_types_;

  // Hooks needed to be called by shill's EventDispatcher.
  EventDispatcher *dispatcher_;
  base::WeakPtrFactory<Config80211> weak_ptr_factory_;
  base::Callback<void(int)> dispatcher_callback_;
  scoped_ptr<IOHandler> dispatcher_handler_;

  Nl80211Socket *sock_;

  DISALLOW_COPY_AND_ASSIGN(Config80211);
};


// Example Config80211 callback object; the callback prints a description of
// each message with its attributes.  You want to make it a singleton so that
// its life isn't dependent on any other object (plus, since this handles
// global events from msg80211, you only want/need one).
class Callback80211Object {
 public:
  Callback80211Object();
  virtual ~Callback80211Object();

  // Get a pointer to the singleton Callback80211Object.
  static Callback80211Object *GetInstance();

  // Install ourselves as a callback.  Done automatically by constructor.
  bool InstallAsCallback();

  // Deinstall ourselves as a callback.  Done automatically by destructor.
  bool DeinstallAsCallback();

  // Simple accessor.
  void set_config80211(Config80211 *config80211) { config80211_ = config80211; }

 protected:
  friend struct base::DefaultLazyInstanceTraits<Callback80211Object>;

 private:
  // When installed, this is the method Config80211 will call when it gets a
  // message from the mac80211 drivers.
  void Config80211MessageCallback(const UserBoundNlMessage &msg);

  Config80211 *config80211_;

  // Config80211MessageCallback method bound to this object to install as a
  // callback.
  base::WeakPtrFactory<Callback80211Object> weak_ptr_factory_;
};

}  // namespace shill

#endif  // SHILL_CONFIG80211_H_

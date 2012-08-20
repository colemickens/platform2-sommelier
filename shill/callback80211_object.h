// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a callback object that observes all nl80211 events that come
// up from the kernel.

#ifndef SHILL_CALLBACK80211_OBJECT_H
#define SHILL_CALLBACK80211_OBJECT_H

#include <iomanip>
#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/lazy_instance.h>

#include "shill/config80211.h"

namespace shill {

class UserBoundNlMessage;

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

  static const char kMetricLinkDisconnectCount[];

  Config80211 *config80211_;

  // Config80211MessageCallback method bound to this object to install as a
  // callback.
  base::WeakPtrFactory<Callback80211Object> weak_ptr_factory_;
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_OBJECT_H

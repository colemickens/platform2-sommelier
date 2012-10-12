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
#include <base/memory/weak_ptr.h>

#include "shill/config80211.h"

namespace shill {

class UserBoundNlMessage;

// Example Config80211 callback object; the callback prints a description of
// each message with its attributes.
class Callback80211Object {
 public:
  explicit Callback80211Object(Config80211 *config80211);
  virtual ~Callback80211Object();

  // Install ourselves as a callback.  Done automatically by constructor.
  bool InstallAsCallback();

  // Deinstall ourselves as a callback.  Done automatically by destructor.
  bool DeinstallAsCallback();

  // TODO(wdg): remove debug code:
  void SetName(std::string name) { name_ = name;}
  const std::string &GetName() { return name_; }

 protected:
  // This is the closure that contains *|this| and a pointer to the message
  // handling callback, below.  It is used in |DeinstallAsCallback|.
  Config80211::Callback callback_;
  Config80211 *config80211_;

 private:
  // TODO(wdg): remove debug code:
  std::string name_;

  // When installed, this is the method Config80211 will call when it gets a
  // message from the mac80211 drivers.
  virtual void Config80211MessageCallback(const UserBoundNlMessage &msg);

  // Config80211MessageCallback method bound to this object to install as a
  // callback.
  base::WeakPtrFactory<Callback80211Object> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Callback80211Object);
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_OBJECT_H

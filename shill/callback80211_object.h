// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a callback object that observes all nl80211 events that come
// up from the kernel.

#ifndef SHILL_CALLBACK80211_OBJECT_H
#define SHILL_CALLBACK80211_OBJECT_H

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>

#include "shill/config80211.h"

namespace shill {

class Nl80211Message;

// Example Config80211 callback object; the callback prints a description of
// each message with its attributes.
class Callback80211Object {
 public:
  Callback80211Object();
  virtual ~Callback80211Object();

  bool InstallAsBroadcastCallback();
  bool DeinstallAsCallback();

  const Config80211::Callback &callback() const { return callback_; }
 protected:
  // When installed, this is the method Config80211 will call when it gets a
  // message from the mac80211 drivers.
  virtual void Config80211MessageCallback(const Nl80211Message &msg);

 private:
  void ReceiveConfig80211Message(const Nl80211Message &msg);

  base::WeakPtrFactory<Callback80211Object> weak_ptr_factory_;
  Config80211::Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(Callback80211Object);
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_OBJECT_H

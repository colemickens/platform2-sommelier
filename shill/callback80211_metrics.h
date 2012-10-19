// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a callback object that observes all nl80211 events that come
// up from the kernel.

#ifndef SHILL_CALLBACK80211_METRICS_H
#define SHILL_CALLBACK80211_METRICS_H

#include <iomanip>
#include <map>
#include <string>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/memory/weak_ptr.h>

#include "shill/callback80211_object.h"

namespace shill {

class Config80211;
class Metrics;
class UserBoundNlMessage;

// Config80211 callback object that sends stuff to UMA metrics.
class Callback80211Metrics : public Callback80211Object {
 public:
  Callback80211Metrics(Config80211 *config80211, Metrics *metrics);

  // Install ourselves as a callback.
  virtual bool InstallAsBroadcastCallback();

 private:
  // When installed, this is the method Config80211 will call when it gets a
  // message from the mac80211 drivers.
  virtual void Config80211MessageCallback(const UserBoundNlMessage &msg);

  static const char kMetricLinkDisconnectCount[];

  Metrics *metrics_;

  // Config80211MessageCallback method bound to this object to install as a
  // callback.
  base::WeakPtrFactory<Callback80211Metrics> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Callback80211Metrics);
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_METRICS_H

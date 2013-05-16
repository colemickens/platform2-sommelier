// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a callback object that observes all nl80211 events that come
// up from the kernel.

#ifndef SHILL_CALLBACK80211_METRICS_H_
#define SHILL_CALLBACK80211_METRICS_H_

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>

#include "shill/netlink_manager.h"

namespace shill {

class Metrics;
class NetlinkManager;
class NetlinkMessage;

// NetlinkManager callback object that sends stuff to UMA metrics.
class Callback80211Metrics :
  public base::SupportsWeakPtr<Callback80211Metrics> {
 public:
  Callback80211Metrics(const NetlinkManager &netlink_manager, Metrics *metrics);

  // Called with each broadcast netlink message that arrives to NetlinkManager.
  // If the message is a deauthenticate message, the method collects the reason
  // for the deauthentication and communicates those to UMA.
  void CollectDisconnectStatistics(const NetlinkMessage &msg);

 private:
  static const char kMetricLinkDisconnectCount[];

  Metrics *metrics_;

  DISALLOW_COPY_AND_ASSIGN(Callback80211Metrics);
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_METRICS_H_

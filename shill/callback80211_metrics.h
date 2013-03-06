// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a callback object that observes all nl80211 events that come
// up from the kernel.

#ifndef SHILL_CALLBACK80211_METRICS_H
#define SHILL_CALLBACK80211_METRICS_H

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>

#include "shill/config80211.h"

namespace shill {

class Config80211;
class Metrics;
class NetlinkMessage;

// Config80211 callback object that sends stuff to UMA metrics.
class Callback80211Metrics :
  public base::SupportsWeakPtr<Callback80211Metrics> {
 public:
  Callback80211Metrics(const Config80211 &config80211, Metrics *metrics);

  // This retrieves the numerical id for the "nl80211" family of messages.
  // Should only be called (once) at initialization time but must be called
  // only after Config80211 knows about the "nl80211" family.
  void InitNl80211FamilyId(const Config80211 &config80211);

  // Called with each broadcast netlink message that arrives to Config80211.
  // If the message is a deauthenticate message, the method collects the reason
  // for the deauthentication and communicates those to UMA.
  void CollectDisconnectStatistics(const NetlinkMessage &msg);

 private:
  static const char kMetricLinkDisconnectCount[];

  Metrics *metrics_;
  uint16_t nl80211_message_type_;

  DISALLOW_COPY_AND_ASSIGN(Callback80211Metrics);
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_METRICS_H

// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class is a callback object that observes all nl80211 events that come
// up from the kernel.

#ifndef SHILL_CALLBACK80211_METRICS_H
#define SHILL_CALLBACK80211_METRICS_H

#include <base/basictypes.h>

#include "shill/callback80211_object.h"

namespace shill {

class Metrics;
class Nl80211Message;

// Config80211 callback object that sends stuff to UMA metrics.
class Callback80211Metrics : public Callback80211Object {
 public:
  explicit Callback80211Metrics(Metrics *metrics);

 protected:
  virtual void Config80211MessageCallback(const Nl80211Message &msg);

 private:
  static const char kMetricLinkDisconnectCount[];

  Metrics *metrics_;

  DISALLOW_COPY_AND_ASSIGN(Callback80211Metrics);
};

}  // namespace shill

#endif  // SHILL_CALLBACK80211_METRICS_H

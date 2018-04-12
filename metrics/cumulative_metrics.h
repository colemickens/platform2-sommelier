// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CumulativeMetrics helps maintain and report "accumulated" quantities, for
// instance how much data has been sent over WiFi and LTE in a day.  Here's
// roughly how a continuously running daemon would do that:
//
// {
//   // initialization, at daemon startup
//   ...
//   std::vector<std::string> stat_names = {"wifi", "lte", "total"};
//   CumulativeMetrics cm(
//     "shill.time.",
//     stat_names,
//     TimeDelta::FromMinutes(5),
//     base::Bind(&UpdateConnectivityStats),
//     TimeDelta::FromDays(1),
//     base::Bind(&ReportConnectivityStats,
//                base::Unretained(metrics_lib_));
//
//   ...
// }
//
// void UpdateConnectivityStats(CumulativeMetrics *cm) {
//   if (wifi_connected) {
//     cm->Add("wifi", cm->ActiveTimeSinceLastUpdate());
//   }
//   if (lte_connected) {
//     cm->Add("lte", cm->ActiveTimeSinceLastUpdate());
//   }
//   cm->Add("total", cm->ActiveTimeSinceLastUpdate());
// }
//
// void ReportConnectivityStats(MetricsLibrary* ml, CumulativeMetrics* cm) {
//   int64_t total = cm->Get("total");
//   ml->SendSample(total, ...);
//   int64_t wifi_time = cm->Get("wifi");
//   ml->SendSample(wifi_time * 100 / total, ...);
//   ...
// }
//
// In the above example, the cumulative metrics object helps maintain three
// quantities (wifi, lte, and total) persistently across boot sessions and
// other daemon restarts.  The quantities are updated every 5 minutes, and
// samples are sent at most once a day.
//
// The class clears (i.e. sets to 0) all accumulated quantities on an OS
// version change.

#ifndef METRICS_CUMULATIVE_METRICS_H_
#define METRICS_CUMULATIVE_METRICS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <base/time/time.h>
#include <base/timer/timer.h>

#include "metrics/persistent_integer.h"

namespace chromeos_metrics {

class CumulativeMetrics {
 public:
  using Callback = base::Callback<void(CumulativeMetrics*)>;
  // Constructor.
  //
  // |prefix| must be unique across all programs and is used to name the files
  // storing the persistent integers.  We recommend using the name of the
  // daemon using this class.  Please ensure that the prefix is not already
  // in use by consulting README.md, and add new prefixes there.
  //
  // |names| are the names of the quantities to be maintained.
  //
  // |update_callback| and |cycle_end_callback| are partial closures which take
  // one argument of type CumulativeMetrics* and return void.  The former is
  // called (roughly) every |update_period_seconds|, and similarly
  // |cycle_end_callback| is called every |accumulation_period_seconds| (see
  // example at the top of this file).
  //
  // Note that the accumulated values are cleared at the end of each cycle
  // after calling |cycle_end_callback_|, which typically sends those
  // quantities as histogram values).  They are also cleared on Chrome OS
  // version changes, but in that case |cycle_end_callback_| is not called
  // (unless the version change is noticed together with the end of a cycle).
  // The assumption is that we want to ship correct histograms for each
  // version, so we can notice the impact of the version change.
  CumulativeMetrics(const std::string& prefix,
                    const std::vector<std::string>& names,
                    base::TimeDelta update_period,
                    Callback update_callback,
                    base::TimeDelta accumulation_period,
                    Callback cycle_end_callback);
  // Returns the time delta (in active time, not elapsed wall clock time) since
  // the last update to the accumulated quantities, or the daemon start.  This
  // is just a convenience function, because it can be easily maintained by the
  // user of this class.  Note that this could be a lot smaller than the
  // elapsed time.
  base::TimeDelta ActiveTimeSinceLastUpdate() const;
  // Sets the value of |name| to |value|.
  void Set(const std::string& name, int64_t value);
  // Adds |value| to the current value of |name|.
  void Add(const std::string& name, int64_t value);
  // Gets the value of |name|
  int64_t Get(const std::string& name) const;
  // Returns the value of |name| and sets it to 0.
  int64_t GetAndClear(const std::string& name);

 private:
  // Called every update_period_seconds_ of active time, or slightly longer.
  // Calls the callback supplied in the constructor.
  void Update();
  // Checks if the current cycle has expired and takes appropriate actions.
  // Returns true if the current cycle has expired, false otherwise.
  bool ProcessCycleEnd();
  // Returns a pointer to the persistent integer for |name| if |name| is a
  // valid cumulative metric.  Otherwise returns nullptr.
  PersistentInteger* Find(const std::string& name) const;
  // Convenience function for reporting uses of invalid metric names.
  void PanicFromBadName(const char* action, const std::string& name) const;

 private:
  std::string prefix_;                    // for persistent integer filenames
  std::map<std::string, std::unique_ptr<PersistentInteger>> values_;
                                          // name -> accumulated value
  base::TimeDelta update_period_;         // interval between update callbacks
  base::TimeDelta accumulation_period_;   // cycle length
  std::unique_ptr<PersistentInteger> cycle_start_;
                                          // clock at beginning of cycle (usecs)
  base::TimeTicks last_update_time_;      // active time at latest update
  // |update_callback_| is called every |update_period_seconds_| to update the
  // accumulators.
  Callback update_callback_;
  // |cycle_end_callback_| is called every |accumulation_period_seconds_| (for
  // instance, one day worth) to send histogram samples.
  Callback cycle_end_callback_;
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(CumulativeMetrics);
};

}  // namespace chromeos_metrics

#endif  // METRICS_CUMULATIVE_METRICS_H_

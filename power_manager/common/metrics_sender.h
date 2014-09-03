// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_COMMON_METRICS_SENDER_H_
#define POWER_MANAGER_COMMON_METRICS_SENDER_H_

#include <string>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <base/memory/scoped_ptr.h>

class MetricsLibraryInterface;

namespace power_manager {

// Stubbable interface for sending metrics.
class MetricsSenderInterface {
 public:
  // Returns the currently-registered singleton. The return value may be NULL
  // (e.g. during testing).
  static MetricsSenderInterface* GetInstance();

  // Registers |instance| as the current singleton. Another instance must not
  // already be registered. Ownership of |instance| remains with the caller.
  static void SetInstance(MetricsSenderInterface* instance);

  virtual ~MetricsSenderInterface() {}

  // See MetricsLibrary::SendToUMA in metrics/metrics_library.h for a
  // description of the arguments in the below methods.

  // Sends a regular (exponential) histogram sample.
  virtual bool SendMetric(const std::string& name,
                          int sample,
                          int min,
                          int max,
                          int num_buckets) = 0;

  // Sends an enumeration (linear) histogram sample.
  virtual bool SendEnumMetric(const std::string& name,
                              int sample,
                              int max) = 0;
};

// MetricsSenderInterface implementation that wraps the metrics library and
// actually forwards metrics to Chrome.
class MetricsSender : public MetricsSenderInterface {
 public:
  // The c'tor and d'tor call SetInstance() to register and unregister this
  // instance.
  explicit MetricsSender(scoped_ptr<MetricsLibraryInterface> metrics_lib);
  virtual ~MetricsSender();

  // MetricsSenderInterface implementation:
  bool SendMetric(const std::string& name,
                  int sample,
                  int min,
                  int max,
                  int num_buckets) override;
  bool SendEnumMetric(const std::string& name, int sample, int max) override;

 private:
  scoped_ptr<MetricsLibraryInterface> metrics_lib_;

  DISALLOW_COPY_AND_ASSIGN(MetricsSender);
};

// Convenience wrapper for calling SendMetric() on the currently-registered
// MetricsSenderInterface singleton. Returns true without doing anything if no
// singleton is currently registered (e.g. for testing).
bool SendMetric(const std::string& name,
                int sample,
                int min,
                int max,
                int num_buckets);

// Convenience wrapper for calling SendEnumMetric() on the currently-registered
// MetricsSenderInterface singleton. Returns true without doing anything if no
// singleton is currently registered (e.g. for testing).
bool SendEnumMetric(const std::string& name, int sample, int max);

}  // namespace power_manager

#endif  // POWER_MANAGER_COMMON_METRICS_SENDER_H_

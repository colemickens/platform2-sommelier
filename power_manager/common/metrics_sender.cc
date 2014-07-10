// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/common/metrics_sender.h"

#include <base/logging.h>
#include <metrics/metrics_library.h>

namespace power_manager {

namespace {

// Singleton instance; weak pointer.
MetricsSenderInterface* instance_ = NULL;

}  // namespace

// static
MetricsSenderInterface* MetricsSenderInterface::GetInstance() {
  return instance_;
}

// static
void MetricsSenderInterface::SetInstance(MetricsSenderInterface* instance) {
  CHECK((!!instance_) ^ (!!instance))
      << "Replacing live instance " << instance_ << " with " << instance;
  instance_ = instance;
}

MetricsSender::MetricsSender(scoped_ptr<MetricsLibraryInterface> metrics_lib)
    : metrics_lib_(metrics_lib.Pass()) {
  MetricsSenderInterface::SetInstance(this);
}

MetricsSender::~MetricsSender() {
  MetricsSenderInterface::SetInstance(NULL);
}

bool MetricsSender::SendMetric(const std::string& name,
                               int sample,
                               int min,
                               int max,
                               int num_buckets) {
  VLOG(1) << "Sending metric " << name << " (sample=" << sample << " min="
          << min << " max=" << max << " num_buckets=" << num_buckets << ")";

  // If the sample falls outside of the histogram's range, just let it end up in
  // the underflow or overflow bucket.
  if (!metrics_lib_->SendToUMA(name, sample, min, max, num_buckets)) {
    LOG(ERROR) << "Failed to send metric " << name;
    return false;
  }
  return true;
}

bool MetricsSender::SendEnumMetric(const std::string& name,
                                   int sample,
                                   int max) {
  VLOG(1) << "Sending enum metric " << name << " (sample=" << sample
          << " max=" << max << ")";

  if (sample > max) {
    LOG(WARNING) << name << " sample " << sample << " is greater than " << max;
    sample = max;
  }

  if (!metrics_lib_->SendEnumToUMA(name, sample, max)) {
    LOG(ERROR) << "Failed to send enum metric " << name;
    return false;
  }
  return true;
}

bool SendMetric(const std::string& name,
                int sample,
                int min,
                int max,
                int num_buckets) {
  MetricsSenderInterface* sender = MetricsSenderInterface::GetInstance();
  return sender ? sender->SendMetric(name, sample, min, max, num_buckets) :
      true;
}

bool SendEnumMetric(const std::string& name, int sample, int max) {
  MetricsSenderInterface* sender = MetricsSenderInterface::GetInstance();
  return sender ? sender->SendEnumMetric(name, sample, max) : true;
}

}  // namespace power_manager

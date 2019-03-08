// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef METRICS_METRICS_LIBRARY_MOCK_H_
#define METRICS_METRICS_LIBRARY_MOCK_H_

#include <string>

#include "metrics/metrics_library.h"

#include <gmock/gmock.h>

class MetricsLibraryMock : public MetricsLibraryInterface {
 public:
  MOCK_METHOD0(Init, void());  // TODO(chromium:940343): Remove this function.
  MOCK_METHOD5(SendToUMA,
               bool(const std::string& name,
                    int sample,
                    int min,
                    int max,
                    int nbuckets));
  MOCK_METHOD3(SendEnumToUMA,
               bool(const std::string& name, int sample, int max));
  MOCK_METHOD2(SendBoolToUMA, bool(const std::string& name, bool sample));
  MOCK_METHOD2(SendSparseToUMA, bool(const std::string& name, int sample));
  MOCK_METHOD1(SendUserActionToUMA, bool(const std::string& action));
#if USE_METRICS_UPLOADER
  MOCK_METHOD6(SendRepeatedToUMA,
               bool(const std::string& name,
                    int sample,
                    int min,
                    int max,
                    int nbuckets,
                    int num_samples));
#endif
  bool AreMetricsEnabled() override { return metrics_enabled_; };
  bool IsGuestMode() override { return guest_mode_; }

  void set_metrics_enabled(bool value) { metrics_enabled_ = value; }
  void set_guest_mode(bool value) { guest_mode_ = value; }

 private:
  bool metrics_enabled_ = true;
  bool guest_mode_ = false;
};

#endif  // METRICS_METRICS_LIBRARY_MOCK_H_

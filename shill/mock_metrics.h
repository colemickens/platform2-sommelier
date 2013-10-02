// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_METRICS_
#define SHILL_MOCK_METRICS_

#include "shill/metrics.h"

#include <gmock/gmock.h>

namespace shill {

class MockMetrics : public Metrics {
 public:
  explicit MockMetrics(EventDispatcher *dispatcher);
  virtual ~MockMetrics();

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD4(AddServiceStateTransitionTimer,
               void(const Service &service, const std::string &histogram_name,
                    Service::ConnectState start_state,
                    Service::ConnectState stop_state));
  MOCK_METHOD1(NotifyDeviceScanStarted, void (int interface_index));
  MOCK_METHOD1(NotifyDeviceScanFinished, void(int interface_index));
  MOCK_METHOD1(ResetScanTimer, void(int interface_index));
  MOCK_METHOD2(NotifyDeviceConnectStarted, void(int interface_index,
                                                bool is_auto_connecting));
  MOCK_METHOD1(NotifyDeviceConnectFinished, void(int interface_index));
  MOCK_METHOD1(ResetConnectTimer, void(int interface_index));
  MOCK_METHOD1(NotifyDefaultServiceChanged, void(const Service *service));
  MOCK_METHOD2(NotifyServiceStateChanged,
               void(const Service &service, Service::ConnectState new_state));
  MOCK_METHOD0(Notify3GPPRegistrationDelayedDropPosted, void());
  MOCK_METHOD0(Notify3GPPRegistrationDelayedDropCanceled, void());
  MOCK_METHOD0(NotifyCorruptedProfile, void());
  MOCK_METHOD3(SendEnumToUMA, bool(const std::string &name, int sample,
                                   int max));
  MOCK_METHOD5(SendToUMA, bool(const std::string &name, int sample, int min,
                               int max, int num_buckets));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMetrics);
};

}  // namespace shill

#endif  // SHILL_MOCK_METRICS_

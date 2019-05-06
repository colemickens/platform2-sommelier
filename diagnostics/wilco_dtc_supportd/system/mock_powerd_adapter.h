// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_SYSTEM_MOCK_POWERD_ADAPTER_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_SYSTEM_MOCK_POWERD_ADAPTER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "diagnostics/wilco_dtc_supportd/system/powerd_adapter.h"

namespace diagnostics {

class MockPowerdAdapter : public PowerdAdapter {
 public:
  MockPowerdAdapter();
  ~MockPowerdAdapter() override;

  MOCK_METHOD1(AddObserver, void(Observer* observer));
  MOCK_METHOD1(RemoveObserver, void(Observer* observer));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPowerdAdapter);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_SYSTEM_MOCK_POWERD_ADAPTER_H_

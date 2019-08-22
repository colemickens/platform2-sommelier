// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_SYSTEM_MOCK_DEBUGD_ADAPTER_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_SYSTEM_MOCK_DEBUGD_ADAPTER_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "diagnostics/wilco_dtc_supportd/system/debugd_adapter.h"

namespace diagnostics {

class MockDebugdAdapter : public DebugdAdapter {
 public:
  MockDebugdAdapter();
  ~MockDebugdAdapter() override;

  MOCK_METHOD1(GetSmartAttributes,
               void(const SmartAttributesCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDebugdAdapter);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_SYSTEM_MOCK_DEBUGD_ADAPTER_H_

// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_EC_EVENT_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_EC_EVENT_SERVICE_H_

#include <base/macros.h>

#include "diagnostics/wilco_dtc_supportd/telemetry/ec_event_service.h"

namespace diagnostics {

class FakeEcEventService : public EcEventService {
 public:
  FakeEcEventService();
  ~FakeEcEventService() override;

  void EmitEcEvent(const EcEventService::EcEvent& ec_event) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEcEventService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_EC_EVENT_SERVICE_H_

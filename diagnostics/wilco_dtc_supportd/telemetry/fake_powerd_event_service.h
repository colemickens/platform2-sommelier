// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_POWERD_EVENT_SERVICE_H_
#define DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_POWERD_EVENT_SERVICE_H_

#include <base/macros.h>
#include <base/observer_list.h>

#include "diagnostics/wilco_dtc_supportd/telemetry/powerd_event_service.h"

namespace diagnostics {

class FakePowerdEventService : public PowerdEventService {
 public:
  FakePowerdEventService();
  ~FakePowerdEventService() override;

  // PowerdEventService overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  bool HasObserver(Observer* observer) const;

  void EmitPowerEvent(PowerdEventService::Observer::PowerEventType type) const;

 private:
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakePowerdEventService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_WILCO_DTC_SUPPORTD_TELEMETRY_FAKE_POWERD_EVENT_SERVICE_H_

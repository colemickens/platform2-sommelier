// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_

#include <memory>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <mojo/public/cpp/bindings/binding_set.h>

#include "diagnostics/cros_healthd/utils/battery_utils.h"
#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Implements the "CrosHealthdService" Mojo interface exposed by the
// cros_healthd daemon (see the API definition at mojo/cros_healthd.mojom)
class CrosHealthdMojoService final
    : public chromeos::cros_healthd::mojom::CrosHealthdService {
 public:
  using ProbeCategoryEnum = chromeos::cros_healthd::mojom::ProbeCategoryEnum;

  // |battery_fetcher| - BatteryFetcher implementation.
  explicit CrosHealthdMojoService(BatteryFetcher* battery_fetcher);
  ~CrosHealthdMojoService() override;

  // chromeos::cros_healthd::mojom::CrosHealthdService overrides:
  void ProbeTelemetryInfo(const std::vector<ProbeCategoryEnum>& categories,
                          const ProbeTelemetryInfoCallback& callback) override;

  // Adds a new binding to the internal binding set.
  void AddBinding(
      chromeos::cros_healthd::mojom::CrosHealthdServiceRequest request);

 private:
  // Mojo binding set that connects |this| with message pipes, allowing the
  // remote ends to call our methods.
  mojo::BindingSet<chromeos::cros_healthd::mojom::CrosHealthdService>
      binding_set_;

  // Unowned pointer that should outlive this CrosHealthdMojoService
  // instance.
  BatteryFetcher* battery_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdMojoService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_

// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_
#define DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_

#include <memory>
#include <vector>

#include <base/callback.h>
#include <base/macros.h>
#include <mojo/public/cpp/bindings/binding.h>

#include "mojo/cros_healthd.mojom.h"

namespace diagnostics {

// Implements the "CrosHealthdService" Mojo interface exposed by the
// cros_healthd daemon (see the API definition at mojo/cros_healthd.mojom)
class CrosHealthdMojoService final
    : public chromeos::cros_healthd::mojom::CrosHealthdService {
 public:
  using ProbeCategoryEnum = chromeos::cros_healthd::mojom::ProbeCategoryEnum;

  // |mojo_pipe_handle| - Pipe to bind this instance to.
  explicit CrosHealthdMojoService(
      mojo::ScopedMessagePipeHandle mojo_pipe_handle);
  ~CrosHealthdMojoService() override;

  // chromeos::cros_healthd::mojom::CrosHealthdService overrides:
  void ProbeTelemetryInfo(const std::vector<ProbeCategoryEnum>& categories,
                          const ProbeTelemetryInfoCallback& callback) override;

  // Set the function that will be called if the binding encounters a connection
  // error.
  void set_connection_error_handler(const base::Closure& error_handler);

 private:
  // Mojo binding that connects |this| with the message pipe, allowing the
  // remote end to call our methods.
  mojo::Binding<chromeos::cros_healthd::mojom::CrosHealthdService>
      self_binding_;

  DISALLOW_COPY_AND_ASSIGN(CrosHealthdMojoService);
};

}  // namespace diagnostics

#endif  // DIAGNOSTICS_CROS_HEALTHD_CROS_HEALTHD_MOJO_SERVICE_H_

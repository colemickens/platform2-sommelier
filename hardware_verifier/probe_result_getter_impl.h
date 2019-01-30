/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_PROBE_RESULT_GETTER_IMPL_H_
#define HARDWARE_VERIFIER_PROBE_RESULT_GETTER_IMPL_H_

#include <memory>

#include <base/macros.h>

#include "hardware_verifier/probe_result_getter.h"

namespace hardware_verifier {

// A helper class to invoke |runtime_probe| service via D-Bus interface.
//
// All methods are mostly implemented by using the existing functions in
// brillo::dbus::* so we mock this helper class and only test other parts
// of |ProbeResultGetterImpl| in unittest.
class RuntimeProbeProxy {
 public:
  RuntimeProbeProxy() = default;
  virtual ~RuntimeProbeProxy() = default;
  virtual bool ProbeCategories(const runtime_probe::ProbeRequest& req,
                               runtime_probe::ProbeResult* resp) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(RuntimeProbeProxy);
};

// The real implementation of |ProbeResultGetter|.
class ProbeResultGetterImpl : public ProbeResultGetter {
 public:
  ProbeResultGetterImpl();

  base::Optional<runtime_probe::ProbeResult> GetFromRuntimeProbe()
      const override;
  base::Optional<runtime_probe::ProbeResult> GetFromFile(
      const base::FilePath& file_path) const override;

 private:
  friend class TestProbeResultGetterImpl;

  explicit ProbeResultGetterImpl(
      std::unique_ptr<RuntimeProbeProxy> runtime_probe_proxy);

  std::unique_ptr<RuntimeProbeProxy> runtime_probe_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ProbeResultGetterImpl);
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_PROBE_RESULT_GETTER_IMPL_H_

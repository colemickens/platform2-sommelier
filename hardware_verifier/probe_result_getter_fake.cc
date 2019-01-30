/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/probe_result_getter_fake.h"

#include <base/files/file_path.h>
#include <base/optional.h>
#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

namespace hardware_verifier {

FakeProbeResultGetter::FakeProbeResultGetter()
    : runtime_probe_run_success_(false),
      runtime_probe_output_(),
      file_probe_results_() {}

base::Optional<runtime_probe::ProbeResult>
FakeProbeResultGetter::GetFromRuntimeProbe() const {
  if (runtime_probe_run_success_) {
    return runtime_probe_output_;
  }
  return base::nullopt;
}

base::Optional<runtime_probe::ProbeResult> FakeProbeResultGetter::GetFromFile(
    const base::FilePath& file_path) const {
  const auto it = file_probe_results_.find(file_path.value());
  if (it == file_probe_results_.end()) {
    return base::nullopt;
  }
  return it->second;
}

void FakeProbeResultGetter::set_runtime_probe_fail() {
  runtime_probe_run_success_ = false;
}

void FakeProbeResultGetter::set_runtime_probe_output(
    const runtime_probe::ProbeResult& data) {
  runtime_probe_run_success_ = true;
  runtime_probe_output_ = data;
}

void FakeProbeResultGetter::set_file_probe_results(
    const FileProbeResults& data) {
  file_probe_results_ = data;
}

}  // namespace hardware_verifier

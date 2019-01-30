/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_PROBE_RESULT_GETTER_FAKE_H_
#define HARDWARE_VERIFIER_PROBE_RESULT_GETTER_FAKE_H_

#include <map>
#include <string>

#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

#include "hardware_verifier/probe_result_getter.h"

namespace hardware_verifier {

// A mock implementation of |ProbeResultGetter| for testing purpose.
class FakeProbeResultGetter : public ProbeResultGetter {
 public:
  using FileProbeResults = std::map<std::string, runtime_probe::ProbeResult>;

  FakeProbeResultGetter();

  base::Optional<runtime_probe::ProbeResult> GetFromRuntimeProbe()
      const override;
  base::Optional<runtime_probe::ProbeResult> GetFromFile(
      const base::FilePath& file_path) const override;

  void set_runtime_probe_fail();
  void set_runtime_probe_output(const runtime_probe::ProbeResult& data);
  void set_file_probe_results(const FileProbeResults& data);

 private:
  bool runtime_probe_run_success_;
  runtime_probe::ProbeResult runtime_probe_output_;
  FileProbeResults file_probe_results_;
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_PROBE_RESULT_GETTER_FAKE_H_

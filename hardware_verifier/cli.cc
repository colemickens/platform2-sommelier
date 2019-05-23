/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/cli.h"

#include <iostream>
#include <string>

#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/optional.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/util/json_util.h>
#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

#include "hardware_verifier/hardware_verifier.pb.h"
#include "hardware_verifier/hw_verification_spec_getter_impl.h"
#include "hardware_verifier/probe_result_getter_impl.h"
#include "hardware_verifier/verifier_impl.h"

namespace hardware_verifier {

// TODO(yhong): Link to the correct class once they are implemented.
CLI::CLI()
    : pr_getter_(std::make_unique<ProbeResultGetterImpl>()),
      vp_getter_(std::make_unique<HwVerificationSpecGetterImpl>()),
      verifier_(std::make_unique<VerifierImpl>()),
      output_stream_(&std::cout) {}

CLIVerificationResult CLI::Run(const std::string& probe_result_file,
                               const std::string& hw_verification_spec_file,
                               const CLIOutputFormat output_format) {
  LOG(INFO) << "Get the probe result.";
  base::Optional<runtime_probe::ProbeResult> probe_result;
  if (probe_result_file.empty()) {
    probe_result = pr_getter_->GetFromRuntimeProbe();
    if (!probe_result) {
      return CLIVerificationResult::kProbeFail;
    }
  } else {
    probe_result = pr_getter_->GetFromFile(base::FilePath(probe_result_file));
    if (!probe_result) {
      return CLIVerificationResult::kInvalidProbeResultFile;
    }
  }

  LOG(INFO) << "Get the verification payload.";
  base::Optional<HwVerificationSpec> hw_verification_spec;
  if (hw_verification_spec_file.empty()) {
    hw_verification_spec = vp_getter_->GetDefault();
  } else {
    hw_verification_spec =
        vp_getter_->GetFromFile(base::FilePath(hw_verification_spec_file));
  }
  if (!hw_verification_spec) {
    return CLIVerificationResult::kInvalidHwVerificationSpecFile;
  }

  LOG(INFO) << "Verify the probe result by the verification payload.";
  const auto verifier_result =
      verifier_->Verify(probe_result.value(), hw_verification_spec.value());
  if (!verifier_result) {
    return CLIVerificationResult::kProbeResultHwVerificationSpecMisalignment;
  }
  const auto hw_verification_report = verifier_result.value();

  LOG(INFO) << "Output the report.";
  switch (output_format) {
    case CLIOutputFormat::kProtoBin:
      if (!hw_verification_report.SerializeToOstream(output_stream_)) {
        return CLIVerificationResult::kUnknownError;
      }
      break;
    case CLIOutputFormat::kJSON:
      auto json_print_opts = google::protobuf::util::JsonPrintOptions();
      json_print_opts.add_whitespace = true;
      json_print_opts.always_print_primitive_fields = true;
      std::string output_data;
      const auto convert_status = google::protobuf::util::MessageToJsonString(
          hw_verification_report, &output_data, json_print_opts);
      if (!convert_status.ok()) {
        LOG(ERROR) << "Failed to transform the report into JSON format: "
                   << convert_status.ToString() << ".";
        return CLIVerificationResult::kUnknownError;
      }
      *output_stream_ << output_data;
      break;
  }

  return (hw_verification_report.is_compliant() ? CLIVerificationResult::kPass
                                                : CLIVerificationResult::kFail);
}

}  // namespace hardware_verifier

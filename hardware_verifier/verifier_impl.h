/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef HARDWARE_VERIFIER_VERIFIER_IMPL_H_
#define HARDWARE_VERIFIER_VERIFIER_IMPL_H_

#include "hardware_verifier/verifier.h"

#include <string>
#include <vector>

#include <google/protobuf/descriptor.h>

namespace hardware_verifier {

class VerifierImpl : public Verifier {
 public:
  VerifierImpl();

  base::Optional<HwVerificationReport> Verify(
      const runtime_probe::ProbeResult& probe_result,
      const HwVerificationSpec& hw_verification_spec) const override;

 private:
  struct CompCategoryInfo {
    // Enum value of of this category.
    int enum_value;

    // Enum name of this category.
    std::string enum_name;

    // The field descriptor of the component list in
    // |runtime_probe::ProbeResult|.
    const google::protobuf::FieldDescriptor* probe_result_comp_field;

    // The field descriptor of the component name in
    // |runtime_probe::ProbeResult|.
    const google::protobuf::FieldDescriptor* probe_result_comp_name_field;

    // The field descriptor of the component values in
    // |runtime_probe::ProbeResult|.
    const google::protobuf::FieldDescriptor* probe_result_comp_values_field;

    // The field descriptor of the component values in |HwVerificationReport|.
    const google::protobuf::FieldDescriptor* report_comp_values_field;
  };

  // An array that records each component category's related info like enum
  // value and name.
  std::vector<CompCategoryInfo> comp_category_infos_;
};

}  // namespace hardware_verifier

#endif  // HARDWARE_VERIFIER_VERIFIER_IMPL_H_

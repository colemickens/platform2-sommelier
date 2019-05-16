/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/verifier_impl.h"

#include <map>
#include <string>
#include <utility>

#include <base/logging.h>
#include <base/optional.h>

namespace hardware_verifier {

base::Optional<HwVerificationReport> VerifierImpl::Verify(
    const runtime_probe::ProbeResult& probe_result,
    const HwVerificationSpec& hw_verification_spec) const {
  // A dictionary which maps (component_category, component_uuid) to its
  // qualification status.
  std::map<int, std::map<std::string, QualificationStatus>> qual_status_dict;
  for (int i = 0; i < hw_verification_spec.component_infos_size(); ++i) {
    const auto& component_info = hw_verification_spec.component_infos(i);
    const auto& category = component_info.component_category();
    const auto& uuid = component_info.component_uuid();
    const auto& qualification_status = component_info.qualification_status();
    const auto& insert_result =
        qual_status_dict[static_cast<int>(category)].emplace(
            uuid, qualification_status);
    if (!insert_result.second) {
      LOG(ERROR)
          << "The verification spec contains duplicated component infos.";
      return base::nullopt;
    }
  }

  HwVerificationReport hw_verification_report;
  hw_verification_report.set_is_compliant(true);

  const auto* category_enum_desc =
      runtime_probe::ProbeRequest_SupportCategory_descriptor();
  const auto* probe_result_refl = probe_result.GetReflection();
  for (int i = 0; i < category_enum_desc->value_count(); ++i) {
    const auto* category_enum_value_desc = category_enum_desc->value(i);
    const auto& category_num = category_enum_value_desc->number();
    const auto& category_name = category_enum_value_desc->name();

    // We expect the enum item name to be identical to the field name in
    // |runtime_probe::ProbeResult| message.
    const auto* field_desc =
        probe_result.GetDescriptor()->FindFieldByName(category_name);
    DCHECK(field_desc) << "Couldn't find the field (" << category_name
                       << ") in |runtime_probe::ProbeResult| message.";
    DCHECK(field_desc->cpp_type() == field_desc->CPPTYPE_MESSAGE &&
           field_desc->is_repeated())
        << "Field (" << category_name
        << ") must be a repeated field for the HW components.";

    const auto& comp_name_to_qual_status = qual_status_dict[category_num];
    for (int j = 0; j < probe_result_refl->FieldSize(probe_result, field_desc);
         ++j) {
      const auto& comp =
          probe_result_refl->GetRepeatedMessage(probe_result, field_desc, j);
      const auto* comp_refl = comp.GetReflection();
      const auto* comp_name_desc =
          comp.GetDescriptor()->FindFieldByName("name");
      DCHECK(comp_name_desc &&
             comp_name_desc->cpp_type() == comp_name_desc->CPPTYPE_STRING &&
             comp_name_desc->is_optional())
          << "Field (" << category_name
          << ") should contain a string of the name of the component.";

      if (!comp_refl->HasField(comp, comp_name_desc)) {
        LOG(ERROR) << "The component name is missing in the probe result.";
        return base::nullopt;
      }
      const auto& comp_name = comp_refl->GetString(comp, comp_name_desc);
      const auto& qual_status_it = comp_name_to_qual_status.find(comp_name);
      if (qual_status_it == comp_name_to_qual_status.end()) {
        LOG(ERROR) << "The probe result contains unregonizable components "
                   << "(category=" << category_name << ", uuid=" << comp_name
                   << ").";
        return base::nullopt;
      }
      auto* found_comp_info =
          hw_verification_report.add_found_component_infos();
      found_comp_info->set_component_category(
          static_cast<runtime_probe::ProbeRequest_SupportCategory>(
              category_num));
      found_comp_info->set_component_uuid(comp_name);
      found_comp_info->set_qualification_status(qual_status_it->second);
      if (qual_status_it->second != QualificationStatus::QUALIFIED) {
        hw_verification_report.set_is_compliant(false);
      }
    }
  }

  // TODO(yhong): Implement the SKU specific checks.
  return hw_verification_report;
}

}  // namespace hardware_verifier

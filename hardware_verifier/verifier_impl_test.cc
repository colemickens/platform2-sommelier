/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>

#include <base/optional.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

#include "hardware_verifier/hardware_verifier.pb.h"
#include "hardware_verifier/verifier_impl.h"

using google::protobuf::util::MessageDifferencer;

namespace hardware_verifier {

namespace {

void SetComponentInfo(
    ComponentInfo* comp,
    const runtime_probe::ProbeRequest_SupportCategory& category,
    const std::string& uuid,
    const QualificationStatus& qualification_status) {
  comp->set_component_category(category);
  comp->set_component_uuid(uuid);
  comp->set_qualification_status(qualification_status);
}

}  // namespace

class TestVerifierImpl : public testing::Test {
 protected:
  void ExpectHwVerificationReportEqual(const HwVerificationReport& lhs,
                                       const HwVerificationReport& rhs) {
    MessageDifferencer differencer;
    differencer.TreatAsSet(
        lhs.GetDescriptor()->FindFieldByName("found_component_infos"));
    differencer.TreatAsSet(
        rhs.GetDescriptor()->FindFieldByName("found_component_infos"));
    EXPECT_TRUE(differencer.Compare(lhs, rhs));
  }
};

TEST_F(TestVerifierImpl, TestVerifySuccAndComplicance) {
  runtime_probe::ProbeResult pr;
  pr.add_battery()->set_name("batt_1");
  pr.add_battery()->set_name("batt_1");
  pr.add_storage()->set_name("storage_A");
  pr.add_storage()->set_name("storage_B");
  pr.add_storage()->set_name("storage_B");
  pr.add_storage()->set_name("storage_C");

  HwVerificationSpec hw_spec;
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::QUALIFIED);
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_A", QualificationStatus::QUALIFIED);
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_B", QualificationStatus::QUALIFIED);
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_C", QualificationStatus::QUALIFIED);
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_audio_codec,
                   "audio_codec_X", QualificationStatus::QUALIFIED);

  HwVerificationReport expected_hw_report;
  expected_hw_report.set_is_compliant(true);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::QUALIFIED);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::QUALIFIED);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_A", QualificationStatus::QUALIFIED);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_B", QualificationStatus::QUALIFIED);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_B", QualificationStatus::QUALIFIED);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_storage,
                   "storage_C", QualificationStatus::QUALIFIED);

  VerifierImpl verifier;
  const auto verify_result = verifier.Verify(pr, hw_spec);
  EXPECT_TRUE(verify_result);
  ExpectHwVerificationReportEqual(expected_hw_report, verify_result.value());
}

TEST_F(TestVerifierImpl, TestVerifySuccAndNotComplicance) {
  runtime_probe::ProbeResult pr;
  pr.add_battery()->set_name("batt_1");

  HwVerificationSpec hw_spec;
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::REJECTED);

  HwVerificationReport expected_hw_report;
  expected_hw_report.set_is_compliant(false);
  SetComponentInfo(expected_hw_report.add_found_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::REJECTED);

  VerifierImpl verifier;
  const auto verify_result = verifier.Verify(pr, hw_spec);
  EXPECT_TRUE(verify_result);
  ExpectHwVerificationReportEqual(expected_hw_report, verify_result.value());
}

TEST_F(TestVerifierImpl, TestVerifyFailProbeResultHasUnknownComponent) {
  runtime_probe::ProbeResult pr;
  pr.add_battery()->set_name("batt_1");
  // "batt_2" is not listed in the verification spec.
  pr.add_battery()->set_name("batt_2");

  HwVerificationSpec hw_spec;
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::QUALIFIED);

  VerifierImpl verifier;
  EXPECT_FALSE(verifier.Verify(pr, hw_spec));
}

TEST_F(TestVerifierImpl, TestVerifyFailProbeResultNameFieldMissing) {
  runtime_probe::ProbeResult pr;
  // This battery component's "name" field is not set.
  pr.add_battery();
  pr.add_battery()->set_name("batt_2");

  HwVerificationSpec hw_spec;
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_2", QualificationStatus::QUALIFIED);

  VerifierImpl verifier;
  EXPECT_FALSE(verifier.Verify(pr, hw_spec));
}

TEST_F(TestVerifierImpl, TestVerifyFailVerificationSpecDuplicate) {
  runtime_probe::ProbeResult pr;
  pr.add_battery()->set_name("batt_1");

  HwVerificationSpec hw_spec;
  // "batt_1" is recorded twice in the verification spec.
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::QUALIFIED);
  SetComponentInfo(hw_spec.add_component_infos(),
                   runtime_probe::ProbeRequest_SupportCategory_battery,
                   "batt_1", QualificationStatus::REJECTED);

  VerifierImpl verifier;
  EXPECT_FALSE(verifier.Verify(pr, hw_spec));
}

}  // namespace hardware_verifier

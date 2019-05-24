/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>
#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

#include "hardware_verifier/hardware_verifier.pb.h"
#include "hardware_verifier/hw_verification_spec_getter_impl.h"
#include "hardware_verifier/probe_result_getter_impl.h"
#include "hardware_verifier/test_utils.h"
#include "hardware_verifier/verifier_impl.h"

using google::protobuf::util::MessageDifferencer;

namespace hardware_verifier {

namespace {

constexpr auto kPrototxtExtension = ".prototxt";

class FakeVbSystemPropertyGetter : public VbSystemPropertyGetter {
 public:
  int GetCrosDebug() const override { return 1; }
};

}  // namespace

class TestVerifierImpl : public testing::Test {
 protected:
  void SetUp() override {
    pr_getter_.reset(new ProbeResultGetterImpl());
    vs_getter_.reset(new HwVerificationSpecGetterImpl(
        std::unique_ptr<VbSystemPropertyGetter>(
            new FakeVbSystemPropertyGetter())));
  }

  void ExpectHwVerificationReportEqual(const HwVerificationReport& lhs,
                                       const HwVerificationReport& rhs) {
    MessageDifferencer differencer;
    // We don't care about the order in the list.
    differencer.TreatAsSet(HwVerificationReport::descriptor()->FindFieldByName(
        "found_component_infos"));
    EXPECT_TRUE(differencer.Compare(lhs, rhs));
  }

  HwVerificationReport LoadHwVerificationReport(
      const base::FilePath& file_path) {
    std::string content;
    EXPECT_TRUE(base::ReadFileToString(file_path, &content));

    HwVerificationReport ret;
    EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(content, &ret));
    return ret;
  }

  HwVerificationSpec LoadHwVerificationSpec(const base::FilePath& file_path) {
    const auto& result = vs_getter_->GetFromFile(file_path);
    EXPECT_TRUE(result);
    return result.value();
  }

  runtime_probe::ProbeResult LoadProbeResult(const base::FilePath& file_path) {
    const auto& result = pr_getter_->GetFromFile(file_path);
    EXPECT_TRUE(result);
    return result.value();
  }

  void TestVerifySuccWithSampleData(const std::string& probe_result_sample_name,
                                    const std::string& spec_sample_name,
                                    const std::string& report_sample_name) {
    const auto& probe_result = LoadProbeResult(GetSampleDataPath().Append(
        probe_result_sample_name + kPrototxtExtension));
    const auto& hw_verification_spec = LoadHwVerificationSpec(
        GetSampleDataPath().Append(spec_sample_name + kPrototxtExtension));
    const auto& expect_hw_verification_report = LoadHwVerificationReport(
        GetSampleDataPath().Append(report_sample_name + kPrototxtExtension));

    VerifierImpl verifier;
    const auto& actual_hw_verification_report =
        verifier.Verify(probe_result, hw_verification_spec);
    EXPECT_TRUE(actual_hw_verification_report);
    ExpectHwVerificationReportEqual(actual_hw_verification_report.value(),
                                    expect_hw_verification_report);
  }

  void TestVerifyFailWithSampleData(const std::string& probe_result_sample_name,
                                    const std::string& spec_sample_name) {
    const auto& probe_result = LoadProbeResult(GetSampleDataPath().Append(
        probe_result_sample_name + kPrototxtExtension));
    const auto& hw_verification_spec = LoadHwVerificationSpec(
        GetSampleDataPath().Append(spec_sample_name + kPrototxtExtension));

    VerifierImpl verifier;
    EXPECT_FALSE(verifier.Verify(probe_result, hw_verification_spec));
  }

  base::FilePath GetSampleDataPath() {
    return GetTestDataPath().Append("verifier_impl_sample_data");
  }

 private:
  std::unique_ptr<ProbeResultGetter> pr_getter_;
  std::unique_ptr<HwVerificationSpecGetter> vs_getter_;
};

TEST_F(TestVerifierImpl, TestVerifySuccWithSample1) {
  TestVerifySuccWithSampleData("probe_result_1", "hw_verification_spec_1",
                               "expect_hw_verification_report_1");
}

TEST_F(TestVerifierImpl, TestVerifySuccWithSample2) {
  TestVerifySuccWithSampleData("probe_result_2", "hw_verification_spec_1",
                               "expect_hw_verification_report_2");
}

TEST_F(TestVerifierImpl, TestVerifySuccWithSample3) {
  TestVerifySuccWithSampleData("probe_result_3", "hw_verification_spec_1",
                               "expect_hw_verification_report_3");
}

TEST_F(TestVerifierImpl, TestVerifyFailWithSample1) {
  TestVerifyFailWithSampleData("probe_result_bad_1", "hw_verification_spec_1");
}

TEST_F(TestVerifierImpl, TestVerifyFailWithSample2) {
  TestVerifyFailWithSampleData("probe_result_bad_2", "hw_verification_spec_1");
}

TEST_F(TestVerifierImpl, TestVerifyFailWithSample3) {
  TestVerifyFailWithSampleData("probe_result_1", "hw_verification_spec_bad_1");
}

}  // namespace hardware_verifier

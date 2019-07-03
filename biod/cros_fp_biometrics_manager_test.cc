// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "biod/cros_fp_biometrics_manager.h"

#include <algorithm>
#include <utility>

#include <base/bind.h>
#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <gtest/gtest.h>

#include "biod/cros_fp_device_interface.h"

namespace biod {

namespace {
constexpr int kMaxTemplateCount = 5;
constexpr char kRecordID[] = "record0";
constexpr char kUserID[] = "0123456789";
constexpr char kLabel[] = "label0";
const std::vector<uint8_t> kFakePositiveMatchSecret1 = {0x00, 0x01, 0x02};
const std::vector<uint8_t> kFakePositiveMatchSecret2 = {0xcc, 0xdd, 0xee, 0xff};
// Validation value corresponding to kFakePositiveMatchSecret1 and kUserID.
const std::vector<uint8_t> kFakeValidationValue1 = {
    0x90, 0xea, 0xfb, 0x75, 0xee, 0x37, 0xeb, 0xb1, 0xb5, 0xe7, 0x81,
    0x47, 0xac, 0xdd, 0xff, 0xbe, 0x20, 0x59, 0x25, 0x24, 0x82, 0xe0,
    0x05, 0xdd, 0x95, 0x09, 0x8e, 0x5a, 0xdc, 0xcc, 0x12, 0x9f,
};
// Validation value corresponding to kFakePositiveMatchSecret2 and kUserID.
const std::vector<uint8_t> kFakeValidationValue2 = {
    0xde, 0xe9, 0x4d, 0xbd, 0xbe, 0x63, 0x8b, 0x9e, 0xc9, 0x25, 0x27,
    0xf1, 0xf6, 0x86, 0x6f, 0xb3, 0x31, 0xf6, 0xb6, 0x52, 0x99, 0x66,
    0x89, 0x88, 0x73, 0x0a, 0xd4, 0x0b, 0xd2, 0x34, 0x7b, 0x71,
};
}  // namespace

class FakeCrosFpDevice : public CrosFpDeviceInterface {
 public:
  FakeCrosFpDevice() { positive_match_secret_ = kFakePositiveMatchSecret1; }
  // CrosFpDeviceInterface overrides:
  ~FakeCrosFpDevice() override = default;

  bool SetFpMode(const FpMode& mode) override { return false; }
  bool GetFpMode(FpMode* mode) override { return false; }
  bool GetFpStats(int* capture_ms, int* matcher_ms, int* overall_ms) override {
    return false;
  }
  bool GetDirtyMap(std::bitset<32>* bitmap) override { return false; }
  bool SupportsPositiveMatchSecret() override { return true; }
  bool GetPositiveMatchSecret(int index, brillo::SecureBlob* secret) override {
    secret->resize(FP_POSITIVE_MATCH_SECRET_BYTES);
    // Zero-pad the secret if it's too short.
    std::fill(secret->begin(), secret->end(), 0);
    std::copy(positive_match_secret_.begin(), positive_match_secret_.end(),
              secret->begin());
    return true;
  }
  bool GetTemplate(int index, VendorTemplate* out) override { return true; }
  bool UploadTemplate(const VendorTemplate& tmpl) override { return false; }
  bool SetContext(std::string user_id) override { return false; }
  bool ResetContext() override { return false; }
  bool InitEntropy(bool reset) override { return false; }

  int MaxTemplateCount() override { return kMaxTemplateCount; }
  int TemplateVersion() override { return FP_TEMPLATE_FORMAT_VERSION; }

  EcCmdVersionSupportStatus EcCmdVersionSupported(uint16_t cmd,
                                                  uint32_t ver) override {
    return EcCmdVersionSupportStatus::UNSUPPORTED;
  }

 private:
  friend class CrosFpBiometricsManagerPeer;
  std::vector<uint8_t> positive_match_secret_;
};

class FakeCrosFpDeviceFactoryImpl : public CrosFpDeviceFactory {
  std::unique_ptr<CrosFpDeviceInterface> Create(
      const MkbpCallback& callback, BiodMetrics* biod_metrics) override {
    return std::make_unique<FakeCrosFpDevice>();
  }
};

// Using a peer class to control access to the class under test is better than
// making the text fixture a friend class.
// http://go/totw/135#using-peers-to-avoid-exposing-implementation
class CrosFpBiometricsManagerPeer {
 public:
  CrosFpBiometricsManagerPeer() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    const scoped_refptr<dbus::MockBus> mock_bus = new dbus::MockBus(options);

    // Set EXPECT_CALL, otherwise gmock forces an failure due to "uninteresting
    // call" because we use StrictMock.
    // https://github.com/google/googletest/blob/fb49e6c164490a227bbb7cf5223b846c836a0305/googlemock/docs/cook_book.md#the-nice-the-strict-and-the-naggy-nicestrictnaggy
    const scoped_refptr<dbus::MockObjectProxy> power_manager_proxy =
        new dbus::MockObjectProxy(
            mock_bus.get(), power_manager::kPowerManagerServiceName,
            dbus::ObjectPath(power_manager::kPowerManagerServicePath));
    EXPECT_CALL(*mock_bus,
                GetObjectProxy(
                    power_manager::kPowerManagerServiceName,
                    dbus::ObjectPath(power_manager::kPowerManagerServicePath)))
        .WillOnce(testing::Return(power_manager_proxy.get()));

    cros_fp_biometrics_manager_ = CrosFpBiometricsManager::Create(
        mock_bus, std::make_unique<FakeCrosFpDeviceFactoryImpl>());
    // Keep a pointer to the fake device to manipulate it later.
    fake_cros_dev_ = static_cast<FakeCrosFpDevice*>(
        cros_fp_biometrics_manager_->cros_dev_.get());
  }

  bool SupportsPositiveMatchSecret() {
    return cros_fp_biometrics_manager_->use_positive_match_secret_;
  }

  void SetUsePositiveMatchSecret(bool use) {
    cros_fp_biometrics_manager_->use_positive_match_secret_ = use;
  }

  void SetDevicePositiveMatchSecret(const std::vector<uint8_t>& new_secret) {
    fake_cros_dev_->positive_match_secret_ = new_secret;
  }

  // Add a record to cros_fp_biometrics_manager_, return the index.
  int AddRecord(int record_format_version,
                const std::string& record_id,
                const std::string& user_id,
                const std::string& label,
                const std::vector<uint8_t>& validation_value) {
    CrosFpBiometricsManager::InternalRecord internal_record = {
        record_format_version, record_id, user_id, label, validation_value};
    cros_fp_biometrics_manager_->records_.emplace_back(
        std::move(internal_record));
    return cros_fp_biometrics_manager_->records_.size() - 1;
  }

  bool ComputeValidationValue(const std::vector<uint8_t>& secret,
                              const std::string& user_id,
                              std::vector<uint8_t>* out) {
    const brillo::SecureBlob secret_blob(secret);
    return cros_fp_biometrics_manager_->ComputeValidationValue(secret_blob,
                                                               user_id, out);
  }

  bool ValidationValueIsCorrect(uint32_t match_idx) {
    return cros_fp_biometrics_manager_->ValidationValueIsCorrect(match_idx);
  }

  BiometricsManager::AttemptMatches CalculateMatches(int match_idx,
                                                     bool matched) {
    return cros_fp_biometrics_manager_->CalculateMatches(match_idx, matched);
  }

 private:
  std::unique_ptr<CrosFpBiometricsManager> cros_fp_biometrics_manager_;
  FakeCrosFpDevice* fake_cros_dev_;
};

class CrosFpBiometricsManagerTest : public ::testing::Test {
 protected:
  CrosFpBiometricsManagerPeer cros_fp_biometrics_manager_peer_;
};

TEST_F(CrosFpBiometricsManagerTest, TestComputeValidationValue) {
  EXPECT_TRUE(cros_fp_biometrics_manager_peer_.SupportsPositiveMatchSecret());
  const std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
      kSecretValidationValuePairs = {
          std::make_pair(kFakePositiveMatchSecret1, kFakeValidationValue1),
          std::make_pair(kFakePositiveMatchSecret2, kFakeValidationValue2),
      };
  for (const auto& pair : kSecretValidationValuePairs) {
    std::vector<uint8_t> validation_value;
    EXPECT_TRUE(cros_fp_biometrics_manager_peer_.ComputeValidationValue(
        pair.first, kUserID, &validation_value));
    EXPECT_EQ(validation_value, pair.second);
  }
}

TEST_F(CrosFpBiometricsManagerTest, TestValidationValueIsCorrect) {
  ASSERT_TRUE(cros_fp_biometrics_manager_peer_.SupportsPositiveMatchSecret());
  cros_fp_biometrics_manager_peer_.SetDevicePositiveMatchSecret(
      kFakePositiveMatchSecret1);
  int index = cros_fp_biometrics_manager_peer_.AddRecord(
      kRecordFormatVersion, kRecordID, kUserID, kLabel, kFakeValidationValue1);
  bool ret = cros_fp_biometrics_manager_peer_.ValidationValueIsCorrect(index);
  EXPECT_TRUE(ret);

  // Make the device return a wrong positive_match_secret.
  cros_fp_biometrics_manager_peer_.SetDevicePositiveMatchSecret(
      kFakePositiveMatchSecret2);
  ret = cros_fp_biometrics_manager_peer_.ValidationValueIsCorrect(index);
  EXPECT_FALSE(ret);
}

TEST_F(CrosFpBiometricsManagerTest, TestCalculateMatchesNotMatched) {
  int index = cros_fp_biometrics_manager_peer_.AddRecord(
      kRecordFormatVersion, kRecordID, kUserID, kLabel, kFakeValidationValue1);
  BiometricsManager::AttemptMatches matches =
      cros_fp_biometrics_manager_peer_.CalculateMatches(index, false);
  // If matched is false then we should report no matches.
  EXPECT_TRUE(matches.empty());
}

TEST_F(CrosFpBiometricsManagerTest, TestCalculateMatchesInvalidIndex) {
  int index = cros_fp_biometrics_manager_peer_.AddRecord(
      kRecordFormatVersion, kRecordID, kUserID, kLabel, kFakeValidationValue1);
  BiometricsManager::AttemptMatches matches =
      cros_fp_biometrics_manager_peer_.CalculateMatches(index + 1, true);
  // If index is invalid then we should report no matches.
  EXPECT_TRUE(matches.empty());
}

TEST_F(CrosFpBiometricsManagerTest,
       TestCalculateMatchesWithPositiveMatchSecret) {
  ASSERT_TRUE(cros_fp_biometrics_manager_peer_.SupportsPositiveMatchSecret());
  int index = cros_fp_biometrics_manager_peer_.AddRecord(
      kRecordFormatVersion, kRecordID, kUserID, kLabel, kFakeValidationValue1);
  BiometricsManager::AttemptMatches matches =
      cros_fp_biometrics_manager_peer_.CalculateMatches(index, true);
  EXPECT_EQ(matches,
            BiometricsManager::AttemptMatches({{kUserID, {kRecordID}}}));
}

TEST_F(CrosFpBiometricsManagerTest,
       TestCalculateMatchesWithoutPositiveMatchSecret) {
  // If not supporting positive match secret, we should just report matches.
  cros_fp_biometrics_manager_peer_.SetUsePositiveMatchSecret(false);
  ASSERT_FALSE(cros_fp_biometrics_manager_peer_.SupportsPositiveMatchSecret());
  int index = cros_fp_biometrics_manager_peer_.AddRecord(
      kRecordFormatVersion, kRecordID, kUserID, kLabel, kFakeValidationValue1);
  BiometricsManager::AttemptMatches matches =
      cros_fp_biometrics_manager_peer_.CalculateMatches(index, true);
  EXPECT_EQ(matches,
            BiometricsManager::AttemptMatches({{kUserID, {kRecordID}}}));
}

}  // namespace biod

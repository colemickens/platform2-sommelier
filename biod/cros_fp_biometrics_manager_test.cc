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
const std::vector<uint8_t> kFakePositiveMatchSecret({0x00, 0x01, 0x02});
}  // namespace

class FakeCrosFpDevice : public CrosFpDeviceInterface {
 public:
  FakeCrosFpDevice() { positive_match_secret_ = kFakePositiveMatchSecret; }
  // CrosFpDeviceInterface overrides:
  ~FakeCrosFpDevice() override = default;

  bool SetFpMode(const FpMode& mode) override { return false; }
  bool GetFpMode(FpMode* mode) override { return false; }
  bool GetFpStats(int* capture_ms, int* matcher_ms, int* overall_ms) override {
    return false;
  }
  bool GetDirtyMap(std::bitset<32>* bitmap) override { return false; }
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

 private:
  std::unique_ptr<CrosFpBiometricsManager> cros_fp_biometrics_manager_;
  FakeCrosFpDevice* fake_cros_dev_;
};

}  // namespace biod

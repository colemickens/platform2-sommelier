// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor_manager.h"

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/file_utils.h>
#include <gtest/gtest.h>

#include "power_manager/common/test_main_loop_runner.h"
#include "power_manager/powerd/system/ambient_light_observer.h"

namespace power_manager {
namespace system {

namespace {

// Abort if it an expected brightness change hasn't been received after this
// many milliseconds.
const int kUpdateTimeoutMs = 5000;

// Frequency with which the ambient light sensor file is polled.
const int kPollIntervalMs = 100;

// Simple AmbientLightObserver implementation that runs the event loop
// until it receives notification that the ambient light level has changed.
class TestObserver : public AmbientLightObserver {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  // Runs |loop_| until OnAmbientLightUpdated() is called.
  bool RunUntilAmbientLightUpdated() {
    return loop_runner_.StartLoop(
        base::TimeDelta::FromMilliseconds(kUpdateTimeoutMs));
  }

  // AmbientLightObserver implementation:
  void OnAmbientLightUpdated(AmbientLightSensorInterface* sensor) override {
    loop_runner_.StopLoop();
  }

 private:
  TestMainLoopRunner loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class AmbientLightSensorManagerTest : public ::testing::Test {
 public:
  AmbientLightSensorManagerTest() {}
  ~AmbientLightSensorManagerTest() override {}

  void SetUp() override {
    CHECK(temp_dir_.CreateUniqueTempDir());
    base::FilePath device0_dir = temp_dir_.GetPath().Append("device0");
    CHECK(base::CreateDirectory(device0_dir));
    data0_file_ = device0_dir.Append("illuminance0_input");
    CHECK(brillo::WriteStringToFile(data0_file_, base::NumberToString(0)));
    base::FilePath loc0_file = device0_dir.Append("location");
    CHECK(brillo::WriteStringToFile(loc0_file, "lid"));
    manager_.reset(new AmbientLightSensorManager());
  }

  void TearDown() override {
    CHECK(base::DeleteFile(temp_dir_.GetPath(), true));
  };

 protected:
  // Temporary directory mimicking a /sys directory containing a set of sensor
  // devices.
  base::ScopedTempDir temp_dir_;

  base::FilePath data0_file_;

  std::unique_ptr<AmbientLightSensorManager> manager_;

  TestObserver internal_backlight_observer_;
  TestObserver keyboard_backlight_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorManagerTest);
};

TEST_F(AmbientLightSensorManagerTest, ZeroSensors) {
  manager_->SetNumSensorsAndInit(0);
  manager_->set_device_list_path_for_testing(temp_dir_.GetPath());
  manager_->Run(false /* read_immediately */);

  EXPECT_EQ(nullptr, manager_->GetSensorForInternalBacklight());
  EXPECT_EQ(nullptr, manager_->GetSensorForKeyboardBacklight());
}

TEST_F(AmbientLightSensorManagerTest, OneSensor) {
  manager_->SetNumSensorsAndInit(1);
  manager_->set_device_list_path_for_testing(temp_dir_.GetPath());
  manager_->set_poll_interval_ms_for_testing(kPollIntervalMs);
  manager_->Run(false /* read_immediately */);

  auto internal_backlight_sensor = manager_->GetSensorForInternalBacklight();
  internal_backlight_sensor->AddObserver(&internal_backlight_observer_);
  ASSERT_TRUE(internal_backlight_observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(0, internal_backlight_sensor->GetAmbientLightLux());
  EXPECT_EQ(data0_file_, internal_backlight_sensor->GetIlluminancePath());
  internal_backlight_sensor->RemoveObserver(&internal_backlight_observer_);

  auto keyboard_backlight_sensor = manager_->GetSensorForKeyboardBacklight();
  keyboard_backlight_sensor->AddObserver(&keyboard_backlight_observer_);
  ASSERT_TRUE(keyboard_backlight_observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(0, keyboard_backlight_sensor->GetAmbientLightLux());
  EXPECT_EQ(data0_file_, keyboard_backlight_sensor->GetIlluminancePath());
  keyboard_backlight_sensor->RemoveObserver(&keyboard_backlight_observer_);
}

TEST_F(AmbientLightSensorManagerTest, TwoSensors) {
  base::FilePath device1_dir = temp_dir_.GetPath().Append("device1");
  CHECK(base::CreateDirectory(device1_dir));
  base::FilePath data1_file = device1_dir.Append("illuminance0_input");
  CHECK(brillo::WriteStringToFile(data1_file, base::NumberToString(1)));
  base::FilePath loc1_file = device1_dir.Append("location");
  CHECK(brillo::WriteStringToFile(loc1_file, "base"));

  manager_->SetNumSensorsAndInit(2);
  manager_->set_device_list_path_for_testing(temp_dir_.GetPath());
  manager_->set_poll_interval_ms_for_testing(kPollIntervalMs);
  manager_->Run(false /* read_immediately */);

  auto internal_backlight_sensor = manager_->GetSensorForInternalBacklight();
  internal_backlight_sensor->AddObserver(&internal_backlight_observer_);
  ASSERT_TRUE(internal_backlight_observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(0, internal_backlight_sensor->GetAmbientLightLux());
  EXPECT_EQ(data0_file_, internal_backlight_sensor->GetIlluminancePath());
  internal_backlight_sensor->RemoveObserver(&internal_backlight_observer_);

  auto keyboard_backlight_sensor = manager_->GetSensorForKeyboardBacklight();
  keyboard_backlight_sensor->AddObserver(&keyboard_backlight_observer_);
  ASSERT_TRUE(keyboard_backlight_observer_.RunUntilAmbientLightUpdated());
  EXPECT_EQ(1, keyboard_backlight_sensor->GetAmbientLightLux());
  EXPECT_EQ(data1_file, keyboard_backlight_sensor->GetIlluminancePath());
  keyboard_backlight_sensor->RemoveObserver(&keyboard_backlight_observer_);
}

}  // namespace system
}  // namespace power_manager

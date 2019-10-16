// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/ambient_light_sensor.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/test/test_mock_time_task_runner.h>
#include <brillo/file_utils.h>
#include <brillo/message_loops/base_message_loop.h>
#include <base/message_loop/message_loop.h>
#include <fuzzer/FuzzedDataProvider.h>

namespace power_manager {
namespace system {

class AmbientLightSensorFuzzer {
 public:
  AmbientLightSensorFuzzer() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~AmbientLightSensorFuzzer() {
    CHECK(base::DeleteFile(temp_dir_.GetPath(), true));
  }

  void SetUp(const uint8_t* data, size_t size) {
    base::FilePath device0_dir = temp_dir_.GetPath().Append("device0");
    CHECK(base::CreateDirectory(device0_dir));
    FuzzedDataProvider data_provider(data, size);

    base::FilePath data0_file_ = device0_dir.Append("illuminance0_input");
    CHECK(brillo::WriteStringToFile(
        data0_file_,
        base::NumberToString(data_provider.ConsumeIntegral<uint32_t>())));

    // Add Color channels.
    base::FilePath color_file = device0_dir.Append("in_illuminance_red_raw");
    CHECK(brillo::WriteStringToFile(
        color_file,
        base::NumberToString(data_provider.ConsumeIntegral<uint32_t>())));
    color_file = device0_dir.Append("in_illuminance_green_raw");
    CHECK(brillo::WriteStringToFile(
        color_file,
        base::NumberToString(data_provider.ConsumeIntegral<uint32_t>())));
    color_file = device0_dir.Append("in_illuminance_blue_raw");
    CHECK(brillo::WriteStringToFile(
        color_file,
        base::NumberToString(data_provider.ConsumeIntegral<uint32_t>())));

    base::FilePath loc0_file_ = device0_dir.Append("location");
    CHECK(brillo::WriteStringToFile(loc0_file_, "lid"));

    sensor_ = std::make_unique<system::AmbientLightSensor>(SensorLocation::LID);
    sensor_->set_device_list_path_for_testing(temp_dir_.GetPath());
  }

  std::unique_ptr<AmbientLightSensor> sensor_;

 protected:
  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AmbientLightSensorFuzzer);
};

}  // namespace system
}  // namespace power_manager

// Disable logging.
struct Environment {
  Environment() { logging::SetMinLogLevel(logging::LOG_FATAL); }
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  // Mock main task runner
  base::MessageLoopForIO message_loop;
  brillo::BaseMessageLoop brillo_loop(&message_loop);
  brillo_loop.SetAsCurrent();

  // Add a TaskRunner where we can control time.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner =
      new base::TestMockTimeTaskRunner();
  message_loop.SetTaskRunner(task_runner);

  auto als_fuzzer =
      std::make_unique<power_manager::system::AmbientLightSensorFuzzer>();
  {
    base::TestMockTimeTaskRunner::ScopedContext scoped_context(
        task_runner.get());
    als_fuzzer->SetUp(data, size);
    als_fuzzer->sensor_->Init(false /* read immediately */);

    // Move time ahead enough so that async file reads occur.
    task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(4000));
  }

  return 0;
}

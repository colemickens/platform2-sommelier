// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Fuzzer for ambient_light_handler
//
// Randomly generate ambient light pref up to 10 steps and test reading
// the mock light sensor up to 10000 times.

#include <base/logging.h>
#include <base/strings/stringprintf.h>
#include <fuzzer/FuzzedDataProvider.h>

#include <vector>

#include "power_manager/powerd/policy/ambient_light_handler.h"
#include "power_manager/powerd/system/ambient_light_sensor_stub.h"

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOG_FATAL);  // Disable logging.
  }
};

namespace power_manager {
namespace policy {

// AmbientLightHandler::Delegate implementation that does nothing.
class FuzzTestDelegate : public AmbientLightHandler::Delegate {
 public:
  FuzzTestDelegate() {}
  ~FuzzTestDelegate() override {}

  double percent() const { return 0; }
  AmbientLightHandler::BrightnessChangeCause cause() const {
    return AmbientLightHandler::BrightnessChangeCause::AMBIENT_LIGHT;
  }

  void SetBrightnessPercentForAmbientLight(
      double brightness_percent,
      AmbientLightHandler::BrightnessChangeCause cause) override {}

  void OnColorTemperatureChanged(int color_temperature) override {}
};

}  // namespace policy
}  // namespace power_manager

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  FuzzedDataProvider data_provider(data, size);

  power_manager::system::AmbientLightSensorStub light_sensor(0);
  power_manager::policy::FuzzTestDelegate delegate;
  power_manager::policy::AmbientLightHandler handler(&light_sensor, &delegate);

  constexpr int kLuxMax = 20000;

  // Generate ambient light steps pref

  int num_step = data_provider.ConsumeIntegralInRange<int>(1, 10);

  std::vector<double> ac;     // AC brightness in percent
  std::vector<double> dc;     // DC brightness in percent
  std::vector<int> lux;       // lux to use in lux_up / lux_down
  std::vector<int> lux_up;    // lux to step up
  std::vector<int> lux_down;  // lux to step down

  for (int i = 0; i < num_step; i++) {
    ac.push_back(data_provider.ConsumeFloatingPointInRange<double>(0.01, 100));
    dc.push_back(data_provider.ConsumeFloatingPointInRange<double>(0.01, 100));
  }
  std::sort(ac.begin(), ac.end());
  std::sort(dc.begin(), dc.end());

  for (int i = 1; i < num_step; i++) {
    lux.push_back(data_provider.ConsumeIntegralInRange<int>(0, kLuxMax));
    lux.push_back(data_provider.ConsumeIntegralInRange<int>(0, kLuxMax));
  }
  std::sort(lux.begin(), lux.end());

  lux_down.push_back(-1);  // Can't step down at lowest level
  for (int i = 0; i < num_step - 1; i++) {
    // lux_downdown[i+1] should be less than or equal to lux_up[i]
    lux_down.push_back(lux[i * 2]);
    lux_up.push_back(lux[i * 2 + 1]);
  }
  lux_up.push_back(-1);  // Can't step up at highest level

  std::string pref;
  for (int i = 0; i < num_step; i++) {
    base::StringAppendF(&pref, "%0.2f %0.2f %d %d\n", ac[i], dc[i], lux_down[i],
                        lux_up[i]);
  }
  pref.erase(pref.length() - 1);  // remove trailing new line

  double initial_brightness_percent =
      data_provider.ConsumeFloatingPointInRange<double>(0, 100);
  double smoothing_constant =
      data_provider.ConsumeFloatingPointInRange<double>(0.01, 1);

  handler.Init(pref, initial_brightness_percent, smoothing_constant);

  int num_readings = data_provider.ConsumeIntegralInRange<int>(0, 10000);
  for (int i = 0; i < num_readings; i++) {
    light_sensor.set_lux(data_provider.ConsumeIntegralInRange<int>(0, kLuxMax));
    handler.OnAmbientLightUpdated(&light_sensor);
  }

  return 0;
}

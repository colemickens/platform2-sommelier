// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <unistd.h>

#include <base/at_exit.h>
#include <base/command_line.h>
#include <base/files/file_descriptor_watcher_posix.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/flag_helper.h>
#include <metrics/metrics_library.h>

#include "power_manager/common/metrics_sender.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#include "power_manager/powerd/daemon.h"
#include "power_manager/powerd/daemon_delegate.h"
#include "power_manager/powerd/policy/external_backlight_controller.h"
#include "power_manager/powerd/policy/internal_backlight_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/system/acpi_wakeup_helper.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"
#include "power_manager/powerd/system/audio_client.h"
#include "power_manager/powerd/system/charge_controller_helper.h"
#include "power_manager/powerd/system/dark_resume.h"
#include "power_manager/powerd/system/dbus_wrapper.h"
#include "power_manager/powerd/system/display/display_power_setter.h"
#include "power_manager/powerd/system/display/display_watcher.h"
#include "power_manager/powerd/system/ec_helper.h"
#include "power_manager/powerd/system/event_device.h"
#include "power_manager/powerd/system/input_watcher.h"
#include "power_manager/powerd/system/internal_backlight.h"
#include "power_manager/powerd/system/lockfile_checker.h"
#include "power_manager/powerd/system/peripheral_battery_watcher.h"
#include "power_manager/powerd/system/pluggable_internal_backlight.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/sar_watcher.h"
#include "power_manager/powerd/system/udev.h"
#include "power_manager/powerd/system/wakeup_device.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

namespace power_manager {

class DaemonDelegateImpl : public DaemonDelegate {
 public:
  DaemonDelegateImpl() = default;
  ~DaemonDelegateImpl() override = default;

  // DaemonDelegate:
  std::unique_ptr<PrefsInterface> CreatePrefs() override {
    auto prefs = std::make_unique<Prefs>();
    CHECK(prefs->Init(Prefs::GetDefaultStore(), Prefs::GetDefaultSources()));
    return prefs;
  }

  std::unique_ptr<system::DBusWrapperInterface> CreateDBusWrapper() override {
    auto wrapper = system::DBusWrapper::Create();
    CHECK(wrapper);
    return wrapper;
  }

  std::unique_ptr<system::UdevInterface> CreateUdev() override {
    auto udev = std::make_unique<system::Udev>();
    CHECK(udev->Init());
    return udev;
  }

  std::unique_ptr<system::AmbientLightSensorInterface>
  CreateAmbientLightSensor() override {
    auto sensor = std::make_unique<system::AmbientLightSensor>();
    sensor->Init(false /* read_immediately */);
    return sensor;
  }

  std::unique_ptr<system::DisplayWatcherInterface> CreateDisplayWatcher(
      system::UdevInterface* udev) override {
    auto watcher = std::make_unique<system::DisplayWatcher>();
    watcher->Init(udev);
    return watcher;
  }

  std::unique_ptr<system::DisplayPowerSetterInterface> CreateDisplayPowerSetter(
      system::DBusWrapperInterface* dbus_wrapper) override {
    auto setter = std::make_unique<system::DisplayPowerSetter>();
    setter->Init(dbus_wrapper);
    return setter;
  }

  std::unique_ptr<policy::BacklightController>
  CreateExternalBacklightController(
      system::DisplayWatcherInterface* display_watcher,
      system::DisplayPowerSetterInterface* display_power_setter,
      system::DBusWrapperInterface* dbus_wrapper) override {
    auto controller = std::make_unique<policy::ExternalBacklightController>();
    controller->Init(display_watcher, display_power_setter, dbus_wrapper);
    return controller;
  }

  std::unique_ptr<system::BacklightInterface> CreateInternalBacklight(
      const base::FilePath& base_path, const std::string& pattern) override {
    auto backlight = std::make_unique<system::InternalBacklight>();
    return backlight->Init(base_path, pattern)
               ? std::move(backlight)
               : std::unique_ptr<system::BacklightInterface>();
  }

  std::unique_ptr<system::BacklightInterface> CreatePluggableInternalBacklight(
      system::UdevInterface* udev,
      const std::string& udev_subsystem,
      const base::FilePath& base_path,
      const std::string& pattern) override {
    auto backlight = std::make_unique<system::PluggableInternalBacklight>();
    backlight->Init(udev, udev_subsystem, base_path, pattern);
    return backlight;
  }

  std::unique_ptr<policy::BacklightController>
  CreateInternalBacklightController(
      system::BacklightInterface* backlight,
      PrefsInterface* prefs,
      system::AmbientLightSensorInterface* sensor,
      system::DisplayPowerSetterInterface* power_setter,
      system::DBusWrapperInterface* dbus_wrapper) override {
    auto controller = std::make_unique<policy::InternalBacklightController>();
    controller->Init(backlight, prefs, sensor, power_setter, dbus_wrapper);
    return controller;
  }

  std::unique_ptr<policy::BacklightController>
  CreateKeyboardBacklightController(
      system::BacklightInterface* backlight,
      PrefsInterface* prefs,
      system::AmbientLightSensorInterface* sensor,
      system::DBusWrapperInterface* dbus_wrapper,
      policy::BacklightController* display_backlight_controller,
      TabletMode initial_tablet_mode) override {
    auto controller = std::make_unique<policy::KeyboardBacklightController>();
    controller->Init(backlight, prefs, sensor, dbus_wrapper,
                     display_backlight_controller, initial_tablet_mode);
    return controller;
  }

  std::unique_ptr<system::InputWatcherInterface> CreateInputWatcher(
      PrefsInterface* prefs, system::UdevInterface* udev) override {
    auto watcher = std::make_unique<system::InputWatcher>();
    CHECK(watcher->Init(std::unique_ptr<system::EventDeviceFactoryInterface>(
                            new system::EventDeviceFactory),
                        std::make_unique<system::WakeupDeviceFactory>(udev),
                        prefs, udev));
    return watcher;
  }

  std::unique_ptr<system::AcpiWakeupHelperInterface> CreateAcpiWakeupHelper()
      override {
    return std::make_unique<system::AcpiWakeupHelper>();
  }

  std::unique_ptr<system::EcHelperInterface> CreateEcHelper() override {
    return std::make_unique<system::EcHelper>();
  }

  std::unique_ptr<system::PeripheralBatteryWatcher>
  CreatePeripheralBatteryWatcher(
      system::DBusWrapperInterface* dbus_wrapper) override {
    auto watcher = std::make_unique<system::PeripheralBatteryWatcher>();
    watcher->Init(dbus_wrapper);
    return watcher;
  }

  std::unique_ptr<system::PowerSupplyInterface> CreatePowerSupply(
      const base::FilePath& power_supply_path,
      PrefsInterface* prefs,
      system::UdevInterface* udev,
      system::DBusWrapperInterface* dbus_wrapper) override {
    auto supply = std::make_unique<system::PowerSupply>();
    supply->Init(power_supply_path, prefs, udev, dbus_wrapper);
    return supply;
  }

  std::unique_ptr<system::SarWatcherInterface> CreateSarWatcher(
      PrefsInterface* prefs, system::UdevInterface* udev) override {
    auto watcher = std::make_unique<system::SarWatcher>();
    watcher->Init(prefs, udev);
    return watcher;
  }

  std::unique_ptr<system::DarkResumeInterface> CreateDarkResume(
      system::PowerSupplyInterface* power_supply,
      PrefsInterface* prefs,
      system::InputWatcherInterface* input_watcher) override {
    auto dark_resume = std::make_unique<system::DarkResume>();
    dark_resume->Init(prefs, input_watcher);
    return dark_resume;
  }

  std::unique_ptr<system::AudioClientInterface> CreateAudioClient(
      system::DBusWrapperInterface* dbus_wrapper) override {
    auto client = std::make_unique<system::AudioClient>();
    client->Init(dbus_wrapper);
    return client;
  }

  std::unique_ptr<system::LockfileCheckerInterface> CreateLockfileChecker(
      const base::FilePath& dir,
      const std::vector<base::FilePath>& files) override {
    return std::make_unique<system::LockfileChecker>(dir, files);
  }

  std::unique_ptr<MetricsSenderInterface> CreateMetricsSender() override {
    auto metrics_lib = std::make_unique<MetricsLibrary>();
    return std::make_unique<MetricsSender>(std::move(metrics_lib));
  }

  std::unique_ptr<system::ChargeControllerHelperInterface>
  CreateChargeControllerHelper() override {
    return std::make_unique<system::ChargeControllerHelper>();
  }

  pid_t GetPid() override { return getpid(); }

  void Launch(const std::string& command) override {
    LOG(INFO) << "Launching \"" << command << "\"";
    pid_t pid = fork();
    if (pid == 0) {
      // TODO(derat): Is this setsid() call necessary?
      setsid();
      // fork() again and exit so that init becomes the command's parent and
      // cleans up when it finally finishes.
      exit(fork() == 0 ? ::system(command.c_str()) : 0);
    } else if (pid > 0) {
      // powerd cleans up after the originally-forked process, which exits
      // immediately after forking again.
      if (waitpid(pid, nullptr, 0) == -1)
        PLOG(ERROR) << "waitpid() on PID " << pid << " failed";
    } else if (pid == -1) {
      PLOG(ERROR) << "fork() failed";
    }
  }

  int Run(const std::string& command) override {
    LOG(INFO) << "Running \"" << command << "\"";
    int return_value = ::system(command.c_str());
    if (return_value == -1) {
      PLOG(ERROR) << "fork() failed";
    } else if (return_value) {
      return_value = WEXITSTATUS(return_value);
      LOG(ERROR) << "Command failed with exit status " << return_value;
    }
    return return_value;
  }

 private:
  base::FilePath read_write_prefs_dir_;
  base::FilePath read_only_prefs_dir_;

  DISALLOW_COPY_AND_ASSIGN(DaemonDelegateImpl);
};

}  // namespace power_manager

int main(int argc, char* argv[]) {
  DEFINE_string(log_dir, "", "Directory where logs are written.");
  DEFINE_string(run_dir, "", "Directory where stateful data is written.");
  // This flag is handled by libbase/libchrome's logging library instead of
  // directly by powerd, but it is defined here so FlagHelper won't abort after
  // seeing an unexpected flag.
  DEFINE_string(vmodule, "",
                "Per-module verbose logging levels, e.g. \"foo=1,bar=2\"");

  brillo::FlagHelper::Init(argc, argv,
                           "powerd, the Chromium OS userspace power manager.");

  CHECK(!FLAGS_log_dir.empty()) << "--log_dir is required";
  CHECK(!FLAGS_run_dir.empty()) << "--run_dir is required";

  const base::FilePath log_file =
      base::FilePath(FLAGS_log_dir)
          .Append(base::StringPrintf(
              "powerd.%s",
              brillo::GetTimeAsLogString(base::Time::Now()).c_str()));
  brillo::UpdateLogSymlinks(
      base::FilePath(FLAGS_log_dir).Append("powerd.LATEST"),
      base::FilePath(FLAGS_log_dir).Append("powerd.PREVIOUS"), log_file);

  logging::LoggingSettings logging_settings;
  logging_settings.logging_dest = logging::LOG_TO_FILE;
  logging_settings.log_file = log_file.value().c_str();
  logging_settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  logging::InitLogging(logging_settings);
  LOG(INFO) << "vcsid " << VCSID;

  // Make it easier to tell if the system just booted, which is useful to know
  // when reading logs from bug reports.
  struct sysinfo info;
  if (sysinfo(&info) == 0) {
    LOG(INFO) << "System uptime: "
              << power_manager::util::TimeDeltaToString(
                     base::TimeDelta::FromSeconds(info.uptime));
  } else {
    PLOG(ERROR) << "sysinfo() failed";
  }

  base::AtExitManager at_exit_manager;
  base::MessageLoopForIO message_loop;
  // This is used in AlarmTimer.
  base::FileDescriptorWatcher watcher{&message_loop};

  power_manager::DaemonDelegateImpl delegate;
  // Extra parens to avoid http://en.wikipedia.org/wiki/Most_vexing_parse.
  power_manager::Daemon daemon(&delegate, (base::FilePath(FLAGS_run_dir)));
  daemon.Init();

  base::RunLoop().Run();
  return 0;
}

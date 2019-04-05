// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_SUSPEND_CONFIGURATOR_H_
#define POWER_MANAGER_POWERD_SYSTEM_SUSPEND_CONFIGURATOR_H_

#include <base/files/file_path.h>
#include <base/macros.h>

namespace power_manager {

class PrefsInterface;

namespace system {

// Interface to configure suspend-related kernel parameters on startup or
// before suspend as needed.
class SuspendConfiguratorInterface {
 public:
  SuspendConfiguratorInterface() = default;
  virtual ~SuspendConfiguratorInterface() = default;

  // Do pre-suspend configuration and logging just before asking kernel to
  // suspend.
  virtual void PrepareForSuspend() = 0;
  // Do post-suspend logging and cleaning just after resuming from suspend.
  virtual void UndoPrepareForSuspend() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuspendConfiguratorInterface);
};

class SuspendConfigurator : public SuspendConfiguratorInterface {
 public:
  // Path to write to enable/disable console during suspend.
  static const base::FilePath kConsoleSuspendPath;

  SuspendConfigurator() = default;
  ~SuspendConfigurator() override = default;

  void Init(PrefsInterface* prefs);

  // SuspendConfiguratorInterface implementation.
  void PrepareForSuspend() override;
  void UndoPrepareForSuspend() override;

  // Sets a prefix path which is used as file system root when testing.
  // Setting to an empty path removes the prefix.
  void set_prefix_path_for_testing(const base::FilePath& file) {
    prefix_path_for_testing_ = file;
  }

 private:
  // Configures whether console should be enabled/disabled during suspend.
  void ConfigureConsoleForSuspend();
  // Returns new FilePath after prepending |prefix_path_for_testing_| to
  // given file path.
  base::FilePath GetPrefixedFilePath(const base::FilePath& file_path) const;

  PrefsInterface* prefs_ = nullptr;  // weak
  // Prefixing all paths for testing with a temp directory. Empty (no
  // prefix) by default.
  base::FilePath prefix_path_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(SuspendConfigurator);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_SUSPEND_CONFIGURATOR_H_
// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SOURCE_H_
#define SETTINGSD_SOURCE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include <base/macros.h>

#include "settingsd/key.h"
#include "settingsd/source_delegate.h"

namespace settingsd {

class SettingsService;

// Setting status values, in most permissive to least permissive order.
enum SettingStatus {
  // Settings from the are considered valid and setting updates are accepted.
  kSettingStatusActive,
  // Any settings provided by the source are already present in the system
  // remain valid. However, no new settings are accepted.
  kSettingStatusWithdrawn,
  // All settings are considered invalid.
  kSettingStatusInvalid,
};

// Decodes a string to the corresponding SettingStatus enum value. If the
// provided string doesn't match one of the valid status names,
// kSettingStatusInvalid will be returned.
SettingStatus SettingStatusFromString(const std::string& status_string);

// Returns the string identifier for the provided setting status.
std::string SettingStatusToString(SettingStatus status);

// Makes a Key for the prefix all configuration for the source identified by
// |source_id| is residing in.
Key MakeSourceKey(const std::string& source_id);

// A high-level description of a configuration source. This defines the
// interface that is used to perform validity checks of a settings documents
// against sources configured in the system.
class Source {
 public:
  explicit Source(const std::string& id);

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  SettingStatus status() const { return status_; }
  const SourceDelegate* delegate() const { return delegate_.get(); }

  // Updates the source from settings.
  //
  // TODO(mnissler): Consider returning information on what changed such that
  // callers don't need to reprocess the entire source definition.
  void Update(const SourceDelegateFactoryFunction& delegate_factory_function,
              const SettingsService& settings);

 private:
  // The source id.
  const std::string id_;

  // Friendly name for the source.
  std::string name_;

  // The current status of this source.
  SettingStatus status_ = kSettingStatusInvalid;

  // The delegate.
  std::unique_ptr<SourceDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(Source);
};

}  // namespace settingsd

#endif  // SETTINGSD_SOURCE_H_

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/source.h"

#include <base/logging.h>
#include <base/values.h>

#include "settingsd/settings_keys.h"
#include "settingsd/settings_service.h"

namespace settingsd {

namespace {

// Setting status strings.
const char* const kSettingStatusStrings[] = {"active", "withdrawn", "invalid"};

}  // namespace

SettingStatus SettingStatusFromString(const std::string& status_string) {
  for (SettingStatus status :
       {kSettingStatusActive, kSettingStatusWithdrawn, kSettingStatusInvalid}) {
    CHECK_LT(status, sizeof(kSettingStatusStrings));
    if (status_string == kSettingStatusStrings[status])
      return status;
  }

  return kSettingStatusInvalid;
}

std::string SettingStatusToString(SettingStatus status) {
  CHECK_LT(status, sizeof(kSettingStatusStrings));
  return kSettingStatusStrings[status];
}

Key MakeSourceKey(const std::string& source_id) {
  // TODO(mnissler): Handle nested sources properly.
  return Key(keys::kSettingsdPrefix).Extend({keys::kSources, source_id});
}

Source::Source(const std::string& id)
    : id_(id), delegate_(new DummySourceDelegate()) {}

void Source::Update(
    const SourceDelegateFactoryFunction& delegate_factory_function,
    const SettingsService& settings) {
  const base::Value* value;

  name_.clear();
  value = settings.GetValue(MakeSourceKey(id_).Extend({keys::sources::kName}));
  if (value)
    value->GetAsString(&name_);

  value =
      settings.GetValue(MakeSourceKey(id_).Extend({keys::sources::kStatus}));
  std::string status_string;
  if (value)
    value->GetAsString(&status_string);
  status_ = SettingStatusFromString(status_string);

  delegate_ = delegate_factory_function(id_, settings);
}

}  // namespace settingsd

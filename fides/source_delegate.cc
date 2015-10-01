// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/source_delegate.h"

#include "fides/locked_settings.h"
#include "fides/settings_keys.h"
#include "fides/settings_service.h"
#include "fides/source.h"

namespace fides {

bool DummySourceDelegate::ValidateVersionComponent(
    const LockedVersionComponent& component) const {
  return false;
}

bool DummySourceDelegate::ValidateContainer(
    const LockedSettingsContainer& container) const {
  return false;
}

std::unique_ptr<SourceDelegate> SourceDelegateFactory::operator()(
    const std::string& source_id,
    const SettingsService& settings) const {
  BlobRef type = settings.GetValue(
      MakeSourceKey(source_id).Extend({keys::sources::kType}));
  if (type.valid()) {
    auto entry = function_map_.find(type.ToString());
    if (entry != function_map_.end())
      return entry->second(source_id, settings);
  }

  // Type invalid or unknown, construct a dummy delegate.
  return std::unique_ptr<SourceDelegate>(new DummySourceDelegate());
}

void SourceDelegateFactory::RegisterFunction(
    const std::string& type,
    const SourceDelegateFactoryFunction& function) {
  function_map_[type] = function;
}

}  // namespace fides

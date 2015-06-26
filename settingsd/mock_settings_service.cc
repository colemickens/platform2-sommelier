// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/mock_settings_service.h"

#include <base/values.h>

#include "settingsd/identifier_utils.h"

namespace settingsd {

MockSettingsService::MockSettingsService() {}

MockSettingsService::~MockSettingsService() {}

const base::Value* MockSettingsService::GetValue(const Key& key) const {
  auto entry = prefix_value_map_.find(key);
  return entry != prefix_value_map_.end() ? entry->second.get() : nullptr;
}

const std::set<Key> MockSettingsService::GetKeys(const Key& prefix) const {
  std::set<Key> result;
  for (const auto& entry : utils::GetRange(prefix, prefix_value_map_))
    result.insert(entry.first);
  return result;
}

void MockSettingsService::AddSettingsObserver(SettingsObserver* observer) {
  observers_.AddObserver(observer);
}

void MockSettingsService::RemoveSettingsObserver(SettingsObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MockSettingsService::SetValue(const Key& key,
                                   std::unique_ptr<base::Value> value) {
  prefix_value_map_[key] = std::move(value);
  std::set<Key> changed_keys;
  changed_keys.insert(key);
  NotifyObservers(changed_keys);
}

void MockSettingsService::NotifyObservers(const std::set<Key>& keys) {
  FOR_EACH_OBSERVER(SettingsObserver, observers_, OnSettingsChanged(keys));
}

}  // namespace settingsd

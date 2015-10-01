// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/mock_settings_service.h"

#include <string>

#include "fides/identifier_utils.h"

namespace fides {

MockSettingsService::MockSettingsService() {}

MockSettingsService::~MockSettingsService() {}

BlobRef MockSettingsService::GetValue(const Key& key) const {
  auto entry = prefix_value_map_.find(key);
  return entry != prefix_value_map_.end() ? BlobRef(&entry->second) : BlobRef();
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
                                   const std::string& value) {
  prefix_value_map_[key] = value;
  std::set<Key> changed_keys;
  changed_keys.insert(key);
  NotifyObservers(changed_keys);
}

void MockSettingsService::NotifyObservers(const std::set<Key>& keys) {
  FOR_EACH_OBSERVER(SettingsObserver, observers_, OnSettingsChanged(keys));
}

}  // namespace fides

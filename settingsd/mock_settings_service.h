// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_MOCK_SETTINGS_SERVICE_H_
#define SETTINGSD_MOCK_SETTINGS_SERVICE_H_

#include <map>
#include <memory>
#include <set>

#include <base/macros.h>
#include <base/observer_list.h>

#include "settingsd/settings_service.h"

namespace base {
class Value;
}  // namespace base

namespace settingsd {

// A trivial SettingsService implementation for testing.
class MockSettingsService : public SettingsService {
 public:
  MockSettingsService();
  virtual ~MockSettingsService();

  // SettingsService:
  const base::Value* GetValue(const Key& key) const override;
  const std::set<Key> GetKeys(const Key& prefix) const override;
  void AddSettingsObserver(SettingsObserver* observer) override;
  void RemoveSettingsObserver(SettingsObserver* observer) override;

  void SetValue(const Key& key, std::unique_ptr<base::Value> value);

  void NotifyObservers(const std::set<Key>& keys);

 private:
  std::map<Key, std::unique_ptr<base::Value>> prefix_value_map_;

  base::ObserverList<SettingsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsService);
};

}  // namespace settingsd

#endif  // SETTINGSD_MOCK_SETTINGS_SERVICE_H_

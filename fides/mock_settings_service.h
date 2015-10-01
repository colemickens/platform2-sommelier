// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_MOCK_SETTINGS_SERVICE_H_
#define FIDES_MOCK_SETTINGS_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include <base/macros.h>
#include <base/observer_list.h>

#include "fides/settings_service.h"

namespace base {
class Value;
}  // namespace base

namespace fides {

// A trivial SettingsService implementation for testing.
class MockSettingsService : public SettingsService {
 public:
  MockSettingsService();
  virtual ~MockSettingsService();

  // SettingsService:
  BlobRef GetValue(const Key& key) const override;
  const std::set<Key> GetKeys(const Key& prefix) const override;
  void AddSettingsObserver(SettingsObserver* observer) override;
  void RemoveSettingsObserver(SettingsObserver* observer) override;

  void SetValue(const Key& key, const std::string& value);

  void NotifyObservers(const std::set<Key>& keys);

 private:
  std::map<Key, std::string> prefix_value_map_;

  base::ObserverList<SettingsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsService);
};

}  // namespace fides

#endif  // FIDES_MOCK_SETTINGS_SERVICE_H_

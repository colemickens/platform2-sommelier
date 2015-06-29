// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SETTINGS_SERVICE_H_
#define SETTINGSD_SETTINGS_SERVICE_H_

#include <set>

#include <base/macros.h>

#include "settingsd/key.h"

namespace base {
class Value;
}  // namespace base

namespace settingsd {

// An observer interface that allows consumers to get notified about setting
// changes.
class SettingsObserver {
 public:
  virtual ~SettingsObserver() = default;

  // Invoked when the observed setting changes. |keys| contains the set of keys
  // that have changed.
  virtual void OnSettingsChanged(const std::set<Key> keys) = 0;
};

// SettingsService is the core API surface of settingsd. It allows consuming
// code to enumerate settings, read setting values and observe setting changes.
class SettingsService {
 public:
  virtual ~SettingsService() = default;

  // Get the value for the specified |key|. Returns nullptr if there is no value
  // present for this key.
  virtual const base::Value* GetValue(const Key& key) const = 0;

  // Get the set of keys the service has values for. Only keys that match the
  // specified |prefix| will be returned. Keys match if they are either
  // identical to prefix or they share the same components as present in
  // |prefix| as a prefix.
  virtual const std::set<Key> GetKeys(const Key& prefix) const = 0;

  // Adds an observer.
  virtual void AddSettingsObserver(SettingsObserver* observer) = 0;

  // Removes an observer.
  virtual void RemoveSettingsObserver(SettingsObserver* observer) = 0;

 private:
  DISALLOW_ASSIGN(SettingsService);
};

}  // namespace settingsd

#endif  // SETTINGSD_SETTINGS_SERVICE_H_

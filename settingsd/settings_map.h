// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SETTINGS_MAP_H_
#define SETTINGSD_SETTINGS_MAP_H_

#include <base/macros.h>
#include <base/values.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "settingsd/settings_document.h"

namespace settingsd {

// Interface for accessing configuration values.
class SettingsMap {
 public:
  virtual ~SettingsMap() {}

  // Clears the settings map.
  virtual void Clear() = 0;

  // Retrieves the currently active value for the setting identified by |key|.
  // If no such setting is currently set, this method returns a nullptr.
  virtual const base::Value* GetValue(const std::string& key) const = 0;

  // Returns a list of currently active settings whose keys have |prefix| as a
  // substring at the beginning.
  virtual std::set<std::string> GetActiveChildKeys(
      const std::string& prefix) const = 0;

  // Returns a list of currently active prefixes have |prefix| as a substring
  // at the beginning.
  virtual std::set<std::string> GetActiveChildPrefixes(
      const std::string& prefix) const = 0;

  // Inserts a settings document into the settings map.
  virtual void InsertDocument(
      std::unique_ptr<const SettingsDocument> document) = 0;

 private:
  DISALLOW_ASSIGN(SettingsMap);
};

}  // namespace settingsd

#endif  // SETTINGSD_SETTINGS_MAP_H_

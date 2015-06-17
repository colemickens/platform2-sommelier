// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SIMPLE_SETTINGS_MAP_H_
#define SETTINGSD_SIMPLE_SETTINGS_MAP_H_

#include <base/macros.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "settingsd/settings_map.h"
#include "settingsd/version_stamp.h"

namespace base {
class Value;
}

namespace settingsd {

class SettingsDocument;

// Every SettingsContainer contains exactly one SettingsDocument.
// SettingsContainer are owned by SettingsMap and their lifetime is controlled
// by a reference count from SettingsMap's |prefix_container_map_| member. Once
// the number of entries in this map referring to a particular container drops
// to zero, the container goes out of scope and is destroyed.
class SettingsContainer {
 public:
  explicit SettingsContainer(std::unique_ptr<const SettingsDocument> document);
  ~SettingsContainer();

  const SettingsDocument& document() const { return *document_; }
  const VersionStamp& GetVersionStamp() const;

 private:
  const std::unique_ptr<const SettingsDocument> document_;

  DISALLOW_COPY_AND_ASSIGN(SettingsContainer);
};

// Simple map-based implemenation of the SettingsMap.
class SimpleSettingsMap : public SettingsMap {
 public:
  SimpleSettingsMap();
  ~SimpleSettingsMap() override;

  // SettingsMap:
  void Clear() override;
  const base::Value* GetValue(const std::string& key) const override;
  std::set<std::string> GetActiveChildKeys(
      const std::string& prefix) const override;
  std::set<std::string> GetActiveChildPrefixes(
      const std::string& prefix) const override;
  void InsertDocument(
      std::unique_ptr<const SettingsDocument> document) override;

 protected:
  // Helper method that deletes all entries in |prefix_container_map_| whose
  // keys lie in the child prefix subspace of |prefix| and where the
  // VersionStamp of the document that is currently providing them is before
  // |upper_limit|. Note that this does not include |prefix| itself.
  void DeleteChildPrefixSpace(const std::string& prefix,
                              const VersionStamp& upper_limit);

  // |prefix_container_map_| maps prefixes to the respective SettingsContainer
  // which is currently providing the active value. The entries in this map
  // control the lifetime of the SettingsContainer: Once the number of entries
  // in this map referring to a particular container drops to zero, the
  // container goes out of scope and is destroyed.
  using PrefixContainerMap =
      std::map<std::string, std::shared_ptr<SettingsContainer>>;
  PrefixContainerMap prefix_container_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleSettingsMap);
};

}  // namespace settingsd

#endif  // SETTINGSD_SIMPLE_SETTINGS_MAP_H_

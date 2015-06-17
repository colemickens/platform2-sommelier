// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_MOCK_SETTINGS_DOCUMENT_H_
#define SETTINGSD_MOCK_SETTINGS_DOCUMENT_H_

#include <base/values.h>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "settingsd/settings_document.h"
#include "settingsd/version_stamp.h"

namespace settingsd {

class MockSettingsDocument : public SettingsDocument {
 public:
  explicit MockSettingsDocument(const VersionStamp& version_stamp);
  ~MockSettingsDocument() override;

  // SettingsDocument:
  const base::Value* GetValue(const std::string& prefix) const override;
  std::set<std::string> GetActiveChildPrefixes(
      const std::string& prefix) const override;
  const VersionStamp& GetVersionStamp() const override;

  void SetEntry(const std::string& prefix, std::unique_ptr<base::Value> value);

 private:
  const VersionStamp version_stamp_;
  std::map<std::string, std::unique_ptr<base::Value>> prefix_value_map_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsDocument);
};

}  // namespace settingsd

#endif  // SETTINGSD_MOCK_SETTINGS_DOCUMENT_H_

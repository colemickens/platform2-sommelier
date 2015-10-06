// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/settings_blob_parser.h"

#include "fides/locked_settings.h"

namespace fides {

SettingsBlobParserRegistry::SettingsBlobParserRegistry() {}

void SettingsBlobParserRegistry::Register(
    const std::string& format,
    const SettingsBlobParserFunction& parser) {
  parsers_[format] = parser;
}

std::unique_ptr<LockedSettingsContainer> SettingsBlobParserRegistry::operator()(
    const std::string& format,
    BlobRef data) const {
  auto entry = parsers_.find(format);
  if (entry != parsers_.end())
    return entry->second(format, data);

  return nullptr;
}

}  // namespace fides

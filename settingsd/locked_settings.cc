// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/locked_settings.h"

#include "settingsd/settings_document.h"

namespace settingsd {

BlobRef LockedSettingsContainer::GetData() const {
  return BlobRef();
}

std::vector<const LockedVersionComponent*>
LockedSettingsContainer::GetVersionComponents() const {
  return std::vector<const LockedVersionComponent*>();
}

// static
std::unique_ptr<SettingsDocument> LockedSettingsContainer::DecodePayload(
    std::unique_ptr<LockedSettingsContainer> container) {
  return container->DecodePayloadInternal();
}

}  // namespace settingsd

// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/locked_settings.h"

#include "fides/settings_document.h"

namespace fides {

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

}  // namespace fides

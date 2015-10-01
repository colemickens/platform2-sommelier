// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/mock_locked_settings.h"

#include "fides/mock_settings_document.h"

namespace fides {

MockLockedVersionComponent::MockLockedVersionComponent() {}

MockLockedVersionComponent::~MockLockedVersionComponent() {}

std::unique_ptr<MockLockedVersionComponent> MockLockedVersionComponent::Clone()
    const {
  std::unique_ptr<MockLockedVersionComponent> copy(
      new MockLockedVersionComponent);
  copy->source_id_ = source_id_;
  copy->valid_ = valid_;
  return copy;
}

std::string MockLockedVersionComponent::GetSourceId() const {
  return source_id_;
}

void MockLockedVersionComponent::SetSourceId(const std::string& source_id) {
  source_id_ = source_id;
}

MockLockedSettingsContainer::MockLockedSettingsContainer(
    std::unique_ptr<MockSettingsDocument> payload)
    : payload_(std::move(payload)) {}

MockLockedSettingsContainer::~MockLockedSettingsContainer() {}

std::unique_ptr<MockLockedSettingsContainer>
MockLockedSettingsContainer::Clone() const {
  std::unique_ptr<MockLockedSettingsContainer> copy(
      new MockLockedSettingsContainer(payload_ ? payload_->Clone() : nullptr));
  for (const auto& entry : version_component_blobs_) {
    copy->version_component_blobs_.insert(
        std::make_pair(entry.first, entry.second->Clone()));
  }
  copy->valid_ = valid_;
  return copy;
}

std::vector<const LockedVersionComponent*>
MockLockedSettingsContainer::GetVersionComponents() const {
  std::vector<const LockedVersionComponent*> result;
  for (auto& entry : version_component_blobs_)
    result.push_back(entry.second.get());
  return result;
}

std::unique_ptr<SettingsDocument>
MockLockedSettingsContainer::DecodePayloadInternal() {
  return std::unique_ptr<SettingsDocument>(std::move(payload_));
}

MockLockedVersionComponent* MockLockedSettingsContainer::GetVersionComponent(
    const std::string& source_id) {
  std::unique_ptr<MockLockedVersionComponent>& blob =
      version_component_blobs_[source_id];
  if (!blob)
    blob.reset(new MockLockedVersionComponent());
  return blob.get();
}

}  // namespace fides

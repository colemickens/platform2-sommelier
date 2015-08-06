// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_MOCK_LOCKED_SETTINGS_H_
#define SETTINGSD_MOCK_LOCKED_SETTINGS_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <base/macros.h>

#include "settingsd/locked_settings.h"
#include "settingsd/settings_document.h"

namespace settingsd {

class MockSettingsDocument;
class VersionStamp;

class MockLockedVersionComponent : public LockedVersionComponent {
 public:
  MockLockedVersionComponent();
  ~MockLockedVersionComponent() override;

  // Returns a copy of this component.
  std::unique_ptr<MockLockedVersionComponent> Clone() const;

  // VersionComponentBlob:
  std::string GetSourceId() const override;

  void SetSourceId(const std::string& source_id);

 private:
  std::string source_id_;

  DISALLOW_COPY_AND_ASSIGN(MockLockedVersionComponent);
};

class MockLockedSettingsContainer : public LockedSettingsContainer {
 public:
  explicit MockLockedSettingsContainer(
      std::unique_ptr<MockSettingsDocument> payload);
  ~MockLockedSettingsContainer() override;

  // Returns a copy of this container.
  std::unique_ptr<MockLockedSettingsContainer> Clone() const;

  // LockedSettingsContainer:
  std::vector<const LockedVersionComponent*> GetVersionComponents()
      const override;
  std::unique_ptr<SettingsDocument> DecodePayloadInternal() override;

  MockLockedVersionComponent* GetVersionComponent(const std::string& source_id);

 private:
  std::unordered_map<std::string, std::unique_ptr<MockLockedVersionComponent>>
      version_component_blobs_;
  std::unique_ptr<MockSettingsDocument> payload_;

  DISALLOW_COPY_AND_ASSIGN(MockLockedSettingsContainer);
};

}  // namespace settingsd

#endif  // SETTINGSD_MOCK_LOCKED_SETTINGS_H_

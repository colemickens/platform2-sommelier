// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_SOURCE_DELEGATE_H_
#define SETTINGSD_SOURCE_DELEGATE_H_

#include <functional>
#include <map>
#include <memory>
#include <string>

#include <base/macros.h>

namespace settingsd {

class SettingsBlob;
class SettingsService;
class VersionComponentBlob;

// A delegate interface for implementing behavior specific to a configuration
// source type. Subclasses implement specific logic for different approaches to
// key management, e.g. device owner keys.
class SourceDelegate {
 public:
  virtual ~SourceDelegate() = default;

  // Validates a version stamp component signed by the source.
  virtual bool ValidateVersionComponentBlob(
      const VersionComponentBlob& blob) const = 0;

  // Validates a blob.
  virtual bool ValidateSettingsBlob(const SettingsBlob& blob) const = 0;

 private:
  DISALLOW_ASSIGN(SourceDelegate);
};

// A trivial SourceDelegate implementation that fails all validation operations.
class DummySourceDelegate : public SourceDelegate {
 public:
  DummySourceDelegate() = default;
  virtual ~DummySourceDelegate() = default;

  // SourceDelegate:
  bool ValidateVersionComponentBlob(
      const VersionComponentBlob& blob) const override;
  bool ValidateSettingsBlob(const SettingsBlob& blob) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummySourceDelegate);
};

// A function type to create source delegates.
using SourceDelegateFactoryFunction =
    std::function<std::unique_ptr<SourceDelegate>(
        const std::string& source_id,
        const SettingsService& settings)>;

// This class implements a type-based source delegate factory function registry.
// Subordinate factory functions can be registered to service delegate creation
// requests for a specified type.
//
// Note that SourceDelegateFactory is not copyable, but can be wrapped in a
// std::reference for use as a SourceDelegateFactoryFunction.
class SourceDelegateFactory {
 public:
  SourceDelegateFactory() = default;

  // Creates a delegate for the given source ID. Looks up the delegate type in
  // |settings|, finds the corresponding factory function, creates a delegate
  // and returns it. Note that a dummy delegate that fails all validation will
  // be returned in case the type is missing or unknown, so callers don't need
  // to worry about handling these cases.
  std::unique_ptr<SourceDelegate> operator()(
      const std::string& source_id,
      const SettingsService& settings) const;

  // Register a new factory function to service
  void RegisterFunction(const std::string& type,
                        const SourceDelegateFactoryFunction& factory);

 private:
  // Maps type identifiers to the corresponding factories.
  std::map<std::string, SourceDelegateFactoryFunction> function_map_;

  DISALLOW_COPY_AND_ASSIGN(SourceDelegateFactory);
};

}  // namespace settingsd

#endif  // SETTINGSD_SOURCE_DELEGATE_H_

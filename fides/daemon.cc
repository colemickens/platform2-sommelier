// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fides/daemon.h"

#include <vector>

#include "fides/dbus_constants.h"
#include "fides/locked_settings.h"
#include "fides/simple_settings_map.h"

namespace fides {

Daemon::Daemon(const ConfigPaths& config_paths)
    : DBusServiceDaemon(kServiceName, kRootServicePath),
      config_paths_(config_paths) {}

bool Daemon::Init() {
  // Attempt to load the trusted document from the storage.
  std::unique_ptr<const SettingsDocument> trusted_document =
      LoadTrustedDocument();
  if (!trusted_document)
    return false;

  // TODO(cschuet): Setup parser_function_ and delegate_factory_function_ here.
  // Instantiate the SettingsDocumentManager for system configuration.
  std::unique_ptr<SimpleSettingsMap> settings_map(new SimpleSettingsMap);
  system_settings_document_manager_.reset(new SettingsDocumentManager(
      parser_function_, delegate_factory_function_,
      config_paths_.system_storage, std::move(settings_map),
      std::move(trusted_document)));

  // Instantiate the DBusSettingsServiceImpl for system configuration.
  dbus_system_settings_service_.reset(new DBusSettingsServiceImpl(
      system_settings_document_manager_.get(), object_manager_->AsWeakPtr(),
      dbus::ObjectPath(kSystemSettingsServicePath)));
  return true;
}

void Daemon::RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) {
  dbus_system_settings_service_->Start(sequencer);
}

std::unique_ptr<const SettingsDocument> Daemon::LoadTrustedDocument() const {
  // TODO(cschuet): Implement trusted document parsing.
  return nullptr;
}

}  // namespace fides

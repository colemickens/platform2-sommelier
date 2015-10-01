// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_DAEMON_H_
#define FIDES_DAEMON_H_

#include <memory>
#include <string>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <chromeos/daemons/dbus_daemon.h>

#include "fides/dbus_settings_service_impl.h"
#include "fides/settings_blob_parser.h"
#include "fides/settings_document_manager.h"
#include "fides/source_delegate.h"

using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::DBusServiceDaemon;

namespace fides {

struct ConfigPaths {
  // Path to directory where settings blobs for system-wide configuration are
  // stored.
  base::FilePath system_storage;

  // Path to file containing the initial trusted document.
  base::FilePath trusted_document;
};

class Daemon : public DBusServiceDaemon {
 public:
  explicit Daemon(const ConfigPaths& config_paths);

  // Initializes the Daemon by loading the trusted document, instantiating the
  // SettingsDocumentManager.
  bool Init();

 protected:
  void RegisterDBusObjectsAsync(AsyncEventSequencer* sequencer) override;

 private:
  // Loads the trusted document from disk.
  std::unique_ptr<const SettingsDocument> LoadTrustedDocument() const;

  // Contains configuration paths.
  const ConfigPaths& config_paths_;

  // Function to create SettingsBlob objects from a blob.
  SettingsBlobParserFunction parser_function_;

  // Function to create source delegates.
  SourceDelegateFactoryFunction delegate_factory_function_;

  // SettingsDocumentManager instance for system-wide configuration.
  std::unique_ptr<SettingsDocumentManager> system_settings_document_manager_;

  // DBusSettingsServiceImpl instance for system-wide configuration.
  std::unique_ptr<DBusSettingsServiceImpl> dbus_system_settings_service_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // namespace fides

#endif  // FIDES_DAEMON_H_

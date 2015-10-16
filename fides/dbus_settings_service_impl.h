// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FIDES_DBUS_SETTINGS_SERVICE_IMPL_H_
#define FIDES_DBUS_SETTINGS_SERVICE_IMPL_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/errors/error.h>

#include "fides/org.chromium.Fides.Settings.h"
#include "fides/settings_service.h"

namespace brillo {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace brillo

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace fides {

class SettingsDocumentManager;

// This class exposes a single instance of SettingsDocumentManager as an
// org::chromium::Fides::Settings service.
class DBusSettingsServiceImpl
    : public org::chromium::Fides::SettingsInterface,
      public SettingsObserver {
 public:
  // The lifetime of |settings_document_manager| is not controlled by this class
  // and thus must be made sure to outlive this instance. |object_path|
  // specifies the D-Bus object path under which this instance can be found.
  DBusSettingsServiceImpl(
      SettingsDocumentManager* settings_document_manager,
      const base::WeakPtr<brillo::dbus_utils::ExportedObjectManager>&
          object_manager,
      const dbus::ObjectPath& object_path);
  ~DBusSettingsServiceImpl() override;

  // SettingsObserver:
  void OnSettingsChanged(const std::set<Key>& keys) override;

  void Start(brillo::dbus_utils::AsyncEventSequencer* sequencer);

 private:
  // org::chromium::Fides::SettingsInterface:
  bool Get(brillo::ErrorPtr* error,
           const std::string& in_key,
           std::vector<uint8_t>* out_value) override;
  bool Enumerate(brillo::ErrorPtr* error,
                 const std::string& in_prefix,
                 std::vector<std::string>* out_values) override;
  bool Update(brillo::ErrorPtr* error,
              const std::vector<uint8_t>& in_blob,
              const std::string& in_source_id) override;

  // The settings document manager that provides data.
  SettingsDocumentManager* settings_document_manager_;

  org::chromium::Fides::SettingsAdaptor dbus_adaptor_{this};
  brillo::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(DBusSettingsServiceImpl);
};

}  // namespace fides

#endif  // FIDES_DBUS_SETTINGS_SERVICE_IMPL_H_

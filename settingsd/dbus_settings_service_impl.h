// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SETTINGSD_DBUS_SETTINGS_SERVICE_IMPL_H_
#define SETTINGSD_DBUS_SETTINGS_SERVICE_IMPL_H_

#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/dbus_object.h>
#include <chromeos/errors/error.h>

#include "settingsd/org.chromium.Settingsd.Settings.h"
#include "settingsd/settings_service.h"

namespace chromeos {
namespace dbus_utils {
class ExportedObjectManager;
}  // namespace dbus_utils
}  // namespace chromeos

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace settingsd {

class SettingsDocumentManager;

// This class exposes a single instance of SettingsDocumentManager as an
// org::chromium::Settingsd::Settings service.
class DBusSettingsServiceImpl
    : public org::chromium::Settingsd::SettingsInterface,
      public SettingsObserver {
 public:
  // The lifetime of |settings_document_manager| is not controlled by this class
  // and thus must be made sure to outlive this instance. |object_path|
  // specifies the D-Bus object path under which this instance can be found.
  DBusSettingsServiceImpl(
      SettingsDocumentManager* settings_document_manager,
      const base::WeakPtr<chromeos::dbus_utils::ExportedObjectManager>&
          object_manager,
      const dbus::ObjectPath& object_path);
  ~DBusSettingsServiceImpl() override;

  // SettingsObserver:
  void OnSettingsChanged(const std::set<Key>& keys) override;

 private:
  // org::chromium::Settingsd::SettingsInterface:
  bool Get(chromeos::ErrorPtr* error,
           const std::string& in_key,
           std::vector<uint8_t>* out_value) override;
  bool Enumerate(chromeos::ErrorPtr* error,
                 const std::string& in_prefix,
                 std::vector<std::string>* out_values) override;
  bool Update(chromeos::ErrorPtr* error,
              const std::vector<uint8_t>& in_blob,
              const std::string& in_source_id) override;

  // The settings document manager that provides data.
  SettingsDocumentManager* settings_document_manager_;

  org::chromium::Settingsd::SettingsAdaptor dbus_adaptor_{this};
  chromeos::dbus_utils::DBusObject dbus_object_;

  DISALLOW_COPY_AND_ASSIGN(DBusSettingsServiceImpl);
};

}  // namespace settingsd

#endif  // SETTINGSD_DBUS_SETTINGS_SERVICE_IMPL_H_

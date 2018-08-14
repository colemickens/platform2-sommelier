// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_MOCK_EXPORTED_OBJECT_MANAGER_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_MOCK_EXPORTED_OBJECT_MANAGER_H_

#include <string>

#include <chromeos/dbus/async_event_sequencer.h>
#include <chromeos/dbus/exported_object_manager.h>
#include <dbus/object_path.h>
#include <gmock/gmock.h>

namespace chromeos {

namespace dbus_utils {

class MockExportedObjectManager : public ExportedObjectManager {
 public:
  using CompletionAction =
      chromeos::dbus_utils::AsyncEventSequencer::CompletionAction;

  using ExportedObjectManager::ExportedObjectManager;
  ~MockExportedObjectManager() override = default;

  MOCK_METHOD1(RegisterAsync,
               void(const CompletionAction& completion_callback));
  MOCK_METHOD3(ClaimInterface,
               void(const dbus::ObjectPath& path,
                    const std::string& interface_name,
                    const ExportedPropertySet::PropertyWriter& writer));
  MOCK_METHOD2(ReleaseInterface,
               void(const dbus::ObjectPath& path,
                    const std::string& interface_name));
};

}  //  namespace dbus_utils

}  //  namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_MOCK_EXPORTED_OBJECT_MANAGER_H_

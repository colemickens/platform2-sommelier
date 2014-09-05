// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/test_util.h"

#include <base/bind.h>
#include <chromeos/dbus/mock_dbus_object.h>
#include <dbus/exported_object.h>
#include <dbus/mock_bus.h>

using chromeos::dbus_utils::MockDBusObject;
using dbus::Bus;
using dbus::ExportedObject;
using dbus::MockBus;
using dbus::ObjectPath;
using std::unique_ptr;
using testing::AnyNumber;

namespace {

const char kTestPath[] = "/some/dbus/path";

void HandleComplete(bool success_is_ignored) {}

}  // namespace


namespace peerd  {
namespace test_util {

unique_ptr<MockDBusObject> MakeMockDBusObject() {
  ObjectPath path(kTestPath);
  Bus::Options options;
  scoped_refptr<MockBus> mock_bus(new MockBus(options));
  EXPECT_CALL(*mock_bus, AssertOnOriginThread()).Times(AnyNumber());
  EXPECT_CALL(*mock_bus, AssertOnDBusThread()).Times(AnyNumber());
  unique_ptr<MockDBusObject> out(
      new MockDBusObject(nullptr, mock_bus, path));
  return out;
}

CompletionAction MakeMockCompletionAction() {
  return base::Bind(&HandleComplete);
}

void HandleMethodExport(
    const std::string& interface_name,
    const std::string& method_name,
    ExportedObject::MethodCallCallback method_call_callback,
    ExportedObject::OnExportedCallback on_exported_callback) {
  on_exported_callback.Run(interface_name, method_name, true);
}

}  // namespace test_util
}  // namespace peerd

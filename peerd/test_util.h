// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_TEST_UTIL_H_
#define PEERD_TEST_UTIL_H_

#include <string>

#include <chromeos/dbus/mock_dbus_object.h>

#include "peerd/typedefs.h"

namespace peerd  {
namespace test_util {

std::unique_ptr<chromeos::dbus_utils::MockDBusObject> MakeMockDBusObject();

CompletionAction MakeMockCompletionAction();

void HandleMethodExport(
    const std::string& interface_name,
    const std::string& method_name,
    dbus::ExportedObject::MethodCallCallback method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback);

}  // namespace test_util
}  // namespace peerd

#endif  // PEERD_TEST_UTIL_H_

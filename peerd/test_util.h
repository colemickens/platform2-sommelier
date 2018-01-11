// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PEERD_TEST_UTIL_H_
#define PEERD_TEST_UTIL_H_

#include <memory>
#include <string>

#include <brillo/dbus/mock_dbus_object.h>
#include <dbus/exported_object.h>
#include <dbus/object_proxy.h>
#include <dbus/message.h>
#include <gmock/gmock.h>

#include "peerd/typedefs.h"

namespace peerd  {
namespace test_util {

MATCHER_P2(IsDBusMethodCallTo, interface, method, "") {
  return arg->GetInterface() == interface &&
         arg->GetMember() == method;
}

std::unique_ptr<brillo::dbus_utils::MockDBusObject> MakeMockDBusObject();

CompletionAction MakeMockCompletionAction();

void HandleMethodExport(
    const std::string& interface_name,
    const std::string& method_name,
    dbus::ExportedObject::MethodCallCallback method_call_callback,
    dbus::ExportedObject::OnExportedCallback on_exported_callback);

void HandleConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback);

dbus::Response* ReturnsEmptyResponse(dbus::MethodCall* method_call,
                                     testing::Unused,
                                     testing::Unused);

}  // namespace test_util
}  // namespace peerd

#endif  // PEERD_TEST_UTIL_H_

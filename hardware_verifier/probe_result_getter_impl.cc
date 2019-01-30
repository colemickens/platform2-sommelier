/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hardware_verifier/probe_result_getter_impl.h"

#include <memory>
#include <string>
#include <utility>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/dbus/dbus_connection.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/errors/error.h>
#include <google/protobuf/text_format.h>
#include <runtime_probe-client/runtime_probe/dbus-constants.h>
#include <runtime_probe/proto_bindings/runtime_probe.pb.h>

#include "hardware_verifier/log_utils.h"

namespace hardware_verifier {

namespace {

const char kTextFmtExt[] = ".prototxt";

bool CheckProbeResultHasError(const runtime_probe::ProbeResult& pr) {
  if (pr.error()) {
    LOG(ERROR) << "The error code in the probe result message is set ("
               << pr.error() << ").";
    return false;
  }
  return true;
}

}  // namespace

ProbeResultGetterImpl::ProbeResultGetterImpl()
    : ProbeResultGetterImpl(std::make_unique<RuntimeProbeProxy>()) {}

ProbeResultGetterImpl::ProbeResultGetterImpl(
    std::unique_ptr<RuntimeProbeProxy> runtime_probe_proxy)
    : runtime_probe_proxy_(std::move(runtime_probe_proxy)) {}

base::Optional<runtime_probe::ProbeResult>
ProbeResultGetterImpl::GetFromRuntimeProbe() const {
  VLOG(1) << "Try to get the probe result by calling |runtime_probe|.";

  // Setup the |ProbeRequest| message, adds all kind of component categories
  // so that |runtime_probe| won't filter out anything.
  // TODO(yhong): Call the proper API to probe components of all categories
  //     directly once b/132127083 is fixed.
  runtime_probe::ProbeRequest probe_request;
  const auto categories_enum_desc =
      runtime_probe::ProbeRequest_SupportCategory_descriptor();
  for (int i = 0; i < categories_enum_desc->value_count(); ++i) {
    const auto category_desc = categories_enum_desc->value(i);
    probe_request.add_categories(
        static_cast<runtime_probe::ProbeRequest_SupportCategory>(
            category_desc->number()));
  }
  VLogProtobuf(2, "ProbeRequest", probe_request);

  runtime_probe::ProbeResult probe_result;
  if (!runtime_probe_proxy_->ProbeCategories(probe_request, &probe_result)) {
    return base::nullopt;
  }
  VLogProtobuf(2, "ProbeResult", probe_result);
  if (!CheckProbeResultHasError(probe_result)) {
    return base::nullopt;
  }
  return probe_result;
}

base::Optional<runtime_probe::ProbeResult> ProbeResultGetterImpl::GetFromFile(
    const base::FilePath& file_path) const {
  VLOG(1) << "Try to load the probe result from file (" << file_path.value()
          << ").";

  if (file_path.Extension() != kTextFmtExt) {
    LOG(ERROR) << "The extension (" << file_path.Extension()
               << ") is unrecognizable.";
    return base::nullopt;
  }

  std::string content;
  if (!base::ReadFileToString(file_path, &content)) {
    LOG(ERROR) << "Failed to read the probe result file.";
    return base::nullopt;
  }

  runtime_probe::ProbeResult probe_result;
  if (!google::protobuf::TextFormat::ParseFromString(content, &probe_result)) {
    LOG(ERROR) << "Failed to parse the probe result in text format.";
    return base::nullopt;
  }
  VLogProtobuf(2, "ProbeResult", probe_result);
  if (!CheckProbeResultHasError(probe_result)) {
    return base::nullopt;
  }
  return probe_result;
}

bool RuntimeProbeProxy::ProbeCategories(
    const runtime_probe::ProbeRequest& req,
    runtime_probe::ProbeResult* resp) const {
  VLOG(1) << "Invoking the D-Bus method ("
          << runtime_probe::kRuntimeProbeInterfaceName
          << "::" << runtime_probe::kProbeCategoriesMethod
          << ") on the service (" << runtime_probe::kRuntimeProbeServiceName
          << ").";
  brillo::DBusConnection dbus_connection;
  auto bus = dbus_connection.Connect();
  auto object_proxy = bus->GetObjectProxy(
      runtime_probe::kRuntimeProbeServiceName,
      dbus::ObjectPath(runtime_probe::kRuntimeProbeServicePath));
  brillo::ErrorPtr error;
  auto dbus_resp = brillo::dbus_utils::CallMethodAndBlock(
      object_proxy, runtime_probe::kRuntimeProbeInterfaceName,
      runtime_probe::kProbeCategoriesMethod, &error, req);
  if (!dbus_resp || !brillo::dbus_utils::ExtractMethodCallResults(
                        dbus_resp.get(), &error, resp)) {
    LOG(ERROR) << "Failed to invoke |runtime_probe| via D-Bus interface ("
               << "code=" << error->GetCode()
               << ", message=" << error->GetMessage() << ").";
    return false;
  }
  return true;
}

}  // namespace hardware_verifier

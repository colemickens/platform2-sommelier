// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/memory/ptr_util.h>
#include <chromeos/dbus/service_constants.h>

#include <google/protobuf/util/json_util.h>

#include "runtime_probe/daemon.h"

namespace runtime_probe {

const char kErrorMsgFailedToPackProtobuf[] = "Failed to serailize the protobuf";

namespace {

// TODO(itspeter): Remove this function.
void AddFakeResponse(ProbeResult* reply) {
  Battery* battery_top_level = reply->add_battery();
  battery_top_level->set_name("generic");
  Battery::Fields* battery_fields = battery_top_level->mutable_values();
  battery_fields->set_index(1);

  std::vector<std::string> virtual_codec_name{
      "ehdaudioXYZ", "dmic-codec", "i2c-MX98373:01", "i2c-MX98373:00"};

  for (auto const& codec_name : virtual_codec_name) {
    AudioCodec* audio_codec_top_level = reply->add_audio_codec();
    audio_codec_top_level->set_name("generic");
    AudioCodec::Fields* audio_codec_fields =
        audio_codec_top_level->mutable_values();
    audio_codec_fields->set_name(codec_name);
  }
}

void DumpProtocolBuffer(const google::protobuf::Message& protobuf,
                        std::string message_name) {
  // TODO(hmchu): b/119938934, Enable those dump only with --debug flag.
  // Using ERROR to show on the /var/log/messages
  VLOG(1) << "---> Protobuf dump of " << message_name;
  VLOG(1) << "       DebugString():\n\n" << protobuf.DebugString();
  std::string json_string;
  google::protobuf::util::JsonPrintOptions options;
  MessageToJsonString(protobuf, &json_string, options);
  VLOG(1) << "       JSON output:\n\n" << json_string << "\n";
  VLOG(1) << "<--- Finished Protobuf dump\n";
}

// Min log level < 0 => --debug flag is passed
bool IsUnderDebug() {
  return logging::GetMinLogLevel() < 0;
}

}  // namespace

Daemon::Daemon() {}

Daemon::~Daemon() {}

void Daemon::ReleaseDbus() {
  CHECK(bus_->ReleaseOwnership(kRuntimeProbeServiceName));
}

int Daemon::OnInit() {
  int exit_code = DBusDaemon::OnInit();
  if (exit_code != EX_OK)
    return exit_code;

  InitDBus();
  return 0;
}

void Daemon::InitDBus() {
  LOG(INFO) << "Init DBus for Runtime Probe";
  // Get or create the ExportedObject for the Runtime Probe service.
  auto const runtime_probe_exported_object =
      bus_->GetExportedObject(dbus::ObjectPath(kRuntimeProbeServicePath));
  CHECK(runtime_probe_exported_object);

  // Register a handler of the ProbeCategories method.
  CHECK(runtime_probe_exported_object->ExportMethodAndBlock(
      kRuntimeProbeInterfaceName, kProbeCategoriesMethod,
      base::Bind(&Daemon::ProbeCategories, weak_ptr_factory_.GetWeakPtr())));

  // Take ownership of the RuntimeProbe service.
  CHECK(bus_->RequestOwnershipAndBlock(kRuntimeProbeServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY));
  LOG(INFO) << kRuntimeProbeServicePath << " DBus initialized.";
}

void Daemon::SendProbeResult(
    const ProbeResult& reply,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender* response_sender) {
  if (IsUnderDebug())
    DumpProtocolBuffer(reply, "ProbeResult");

  std::unique_ptr<dbus::Response> message(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(message.get());
  if (!writer.AppendProtoAsArrayOfBytes(reply)) {
    LOG(ERROR) << kErrorMsgFailedToPackProtobuf;
    response_sender->Run(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_INVALID_ARGS, kErrorMsgFailedToPackProtobuf));
  } else {
    // TODO(itspeter): b/119939408, PII filter before return.
    response_sender->Run(std::move(message));
    // TODO(itspeter): b/119937341, Exit program gracefully.
  }
}

void Daemon::ProbeCategories(
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> message(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(message.get());
  ProbeRequest request;
  ProbeResult reply;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    reply.set_error(RUNTIME_PROBE_ERROR_PROBE_REQUEST_INVALID);
    return SendProbeResult(reply, method_call, &response_sender);
  }

  if (IsUnderDebug())
    DumpProtocolBuffer(request, "ProbeRequest");

  // TODO(itspeter): Compose real response base on probe result.
  AddFakeResponse(&reply);
  return SendProbeResult(reply, method_call, &response_sender);
}

}  // namespace runtime_probe

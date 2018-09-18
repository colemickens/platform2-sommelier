// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/oobe_config_restore_service.h"

#include <string>

#include "oobe_config/proto_bindings/oobe_config.pb.h"

using brillo::dbus_utils::AsyncEventSequencer;

namespace oobe_config {

namespace {

// Serializes |proto| to the byte array |proto_blob|.
template <typename Proto>
bool SerializeProtoToBlob(const Proto& proto, ProtoBlob* proto_blob) {
  DCHECK(proto_blob);
  proto_blob->resize(proto.ByteSizeLong());
  return proto.SerializeToArray(proto_blob->data(), proto.ByteSizeLong());
}

}  // namespace

OobeConfigRestoreService::OobeConfigRestoreService(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object)
    : org::chromium::OobeConfigRestoreAdaptor(this),
      dbus_object_(std::move(dbus_object)) {}

OobeConfigRestoreService::~OobeConfigRestoreService() = default;

void OobeConfigRestoreService::RegisterAsync(
    const AsyncEventSequencer::CompletionAction& completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

void OobeConfigRestoreService::ProcessAndGetOobeAutoConfig(
    int32_t* error, ProtoBlob* oobe_config_blob) {
  DCHECK(error);
  DCHECK(oobe_config_blob);

  // TODO(zentaro): For now this just returns empty config.
  std::string chrome_config_json;
  OobeRestoreData data_proto;
  data_proto.set_chrome_config_json(chrome_config_json);
  *error = SerializeProtoToBlob(data_proto, oobe_config_blob) ? 0 : -1;
}

}  // namespace oobe_config

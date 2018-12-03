// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/key_challenge_service_impl.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include <base/bind.h>
#include <base/logging.h>
#include <brillo/errors/error.h>
#include <dbus/bus.h>
#include <google/protobuf/message_lite.h>

namespace cryptohome {

namespace {

std::vector<uint8_t> SerializeProto(
    const google::protobuf::MessageLite& proto) {
  std::vector<uint8_t> serialized_proto(proto.ByteSizeLong());
  CHECK(
      proto.SerializeToArray(serialized_proto.data(), serialized_proto.size()));
  return serialized_proto;
}

bool DeserializeProto(const std::vector<uint8_t>& raw_buf,
                      google::protobuf::MessageLite* proto) {
  return proto->ParseFromArray(raw_buf.data(), raw_buf.size());
}

void OnDBusChallengeKeySuccess(
    const KeyChallengeService::ResponseCallback& original_callback,
    const std::vector<uint8_t>& challenge_response) {
  if (challenge_response.empty()) {
    original_callback.Run(nullptr /* response */);
    return;
  }
  auto response_proto = std::make_unique<KeyChallengeResponse>();
  if (!DeserializeProto(challenge_response, response_proto.get())) {
    LOG(ERROR)
        << "Failed to parse KeyChallengeResponse from ChallengeKey D-Bus call";
    original_callback.Run(nullptr /* response */);
    return;
  }
  original_callback.Run(std::move(response_proto));
}

void OnDBusChallengeKeyFailure(
    const KeyChallengeService::ResponseCallback& original_callback,
    brillo::Error* /* error */) {
  original_callback.Run(nullptr /* response */);
}

}  // namespace

KeyChallengeServiceImpl::KeyChallengeServiceImpl(
    scoped_refptr<dbus::Bus> dbus_bus,
    const std::string& key_delegate_dbus_service_name)
    : dbus_proxy_(dbus_bus, key_delegate_dbus_service_name) {
  DCHECK(dbus_bus);
  DCHECK(!key_delegate_dbus_service_name.empty());
}

KeyChallengeServiceImpl::~KeyChallengeServiceImpl() = default;

void KeyChallengeServiceImpl::ChallengeKey(
    const AccountIdentifier& account_id,
    const KeyChallengeRequest& key_challenge_request,
    const ResponseCallback& response_callback) {
  dbus_proxy_.ChallengeKeyAsync(
      SerializeProto(account_id), SerializeProto(key_challenge_request),
      base::Bind(&OnDBusChallengeKeySuccess, response_callback),
      base::Bind(&OnDBusChallengeKeyFailure, response_callback));
}

}  // namespace cryptohome

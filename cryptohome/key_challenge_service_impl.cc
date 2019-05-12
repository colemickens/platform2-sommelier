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
#include <base/optional.h>
#include <brillo/errors/error.h>
#include <dbus/bus.h>
#include <google/protobuf/message_lite.h>

namespace cryptohome {

namespace {

// Used for holding OnceCallback when multiple callback function needs it, but
// only one of them will run. Note: This is not thread safe.
template <typename T>
class OnceCallbackHolder {
 public:
  explicit OnceCallbackHolder(T obj) : obj_(std::move(obj)) {}

  T get() {
    base::Optional<T> res;
    std::swap(res, obj_);
    DCHECK(res.has_value());
    return std::move(res.value());
  }

 private:
  // The object that we are holding
  base::Optional<T> obj_;
};

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
    std::shared_ptr<OnceCallbackHolder<KeyChallengeService::ResponseCallback>>
        callback_holder,
    const std::vector<uint8_t>& challenge_response) {
  KeyChallengeService::ResponseCallback original_callback =
      callback_holder->get();
  if (challenge_response.empty()) {
    std::move(original_callback).Run(nullptr /* response */);
    return;
  }
  auto response_proto = std::make_unique<KeyChallengeResponse>();
  if (!DeserializeProto(challenge_response, response_proto.get())) {
    LOG(ERROR)
        << "Failed to parse KeyChallengeResponse from ChallengeKey D-Bus call";
    std::move(original_callback).Run(nullptr /* response */);
    return;
  }
  std::move(original_callback).Run(std::move(response_proto));
}

void OnDBusChallengeKeyFailure(
    std::shared_ptr<OnceCallbackHolder<KeyChallengeService::ResponseCallback>>
        callback_holder,
    brillo::Error* /* error */) {
  KeyChallengeService::ResponseCallback original_callback =
      callback_holder->get();
  std::move(original_callback).Run(nullptr /* response */);
}

}  // namespace

KeyChallengeServiceImpl::KeyChallengeServiceImpl(
    scoped_refptr<dbus::Bus> dbus_bus,
    const std::string& key_delegate_dbus_service_name)
    : key_delegate_dbus_service_name_(key_delegate_dbus_service_name),
      dbus_proxy_(dbus_bus, key_delegate_dbus_service_name_) {
  DCHECK(dbus_bus);
  DCHECK(!key_delegate_dbus_service_name_.empty());
}

KeyChallengeServiceImpl::~KeyChallengeServiceImpl() = default;

void KeyChallengeServiceImpl::ChallengeKey(
    const AccountIdentifier& account_id,
    const KeyChallengeRequest& key_challenge_request,
    ResponseCallback response_callback) {
  if (!dbus_validate_bus_name(key_delegate_dbus_service_name_.c_str(),
                              nullptr /* error */)) {
    // Bail out to avoid crashing inside the D-Bus library.
    // TODO(emaxx): Remove this special handling once libchrome is uprev'ed to
    // include the fix from crbug.com/927196.
    LOG(ERROR) << "Invalid key challenge service name";
    std::move(response_callback).Run(nullptr /* response */);
    return;
  }
  std::shared_ptr<OnceCallbackHolder<ResponseCallback>> callback_holder(
      new OnceCallbackHolder<ResponseCallback>(std::move(response_callback)));
  dbus_proxy_.ChallengeKeyAsync(
      SerializeProto(account_id), SerializeProto(key_challenge_request),
      base::Bind(&OnDBusChallengeKeySuccess, callback_holder),
      base::Bind(&OnDBusChallengeKeyFailure, callback_holder));
}

}  // namespace cryptohome

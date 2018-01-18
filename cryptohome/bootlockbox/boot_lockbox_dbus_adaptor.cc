// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/bootlockbox/boot_lockbox_dbus_adaptor.h"

#include <memory>
#include <string>
#include <vector>

#include <base/logging.h>
#include <brillo/errors/error.h>
#include <brillo/errors/error_codes.h>
#include <dbus/dbus-protocol.h>

#include "rpc.pb.h"  // NOLINT(build/include)

namespace {
// Creates a dbus error message.
brillo::ErrorPtr CreateError(const std::string& code,
                             const std::string& message) {
  return brillo::Error::Create(FROM_HERE, brillo::errors::dbus::kDomain,
                               code, message);
}

// Serializes BaseReply protobuf to vector.
std::vector<uint8_t> ReplyToVector(const cryptohome::BaseReply& reply) {
  std::vector<uint8_t> reply_vec(reply.ByteSize());
  reply.SerializeToArray(reply_vec.data(), reply_vec.size());
  return reply_vec;
}

}  // namespace

namespace cryptohome {

BootLockboxDBusAdaptor::BootLockboxDBusAdaptor(scoped_refptr<dbus::Bus> bus,
                                               BootLockbox* boot_lockbox)
    : org::chromium::BootLockboxInterfaceAdaptor(this),
      boot_lockbox_(boot_lockbox),
      dbus_object_(nullptr, bus,
          org::chromium::BootLockboxInterfaceAdaptor::GetObjectPath()) {}

void BootLockboxDBusAdaptor::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction& cb) {
  RegisterWithDBusObject(&dbus_object_);
  dbus_object_.RegisterAsync(cb);
}

void BootLockboxDBusAdaptor::SignBootLockbox(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        std::vector<uint8_t>>> response,
    const std::vector<uint8_t>& in_request) {
  cryptohome::SignBootLockboxRequest request_pb;
  if (!request_pb.ParseFromArray(in_request.data(), in_request.size()) ||
      !request_pb.has_data()) {
    brillo::ErrorPtr error = CreateError(DBUS_ERROR_INVALID_ARGS,
        "SignBootLockboxRequest has invalid argument(s).");
    response->ReplyWithError(error.get());
    return;
  }

  cryptohome::BaseReply reply;
  brillo::SecureBlob signature;
  if (!boot_lockbox_->Sign(brillo::SecureBlob(request_pb.data()), &signature)) {
    reply.set_error(cryptohome::CRYPTOHOME_ERROR_LOCKBOX_CANNOT_SIGN);
  } else {
    reply.MutableExtension(
        cryptohome::SignBootLockboxReply::reply)->set_signature(
            signature.to_string());
  }
  response->Return(ReplyToVector(reply));
}

void BootLockboxDBusAdaptor::VerifyBootLockbox(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
        std::vector<uint8_t>>> response,
    const std::vector<uint8_t>& in_request) {
  cryptohome::VerifyBootLockboxRequest request_pb;
  if (!request_pb.ParseFromArray(in_request.data(), in_request.size()) ||
      !request_pb.has_data()) {
    brillo::ErrorPtr error = CreateError(DBUS_ERROR_INVALID_ARGS,
        "VerifyBootLockboxRequest has invalid argument(s).");
    response->ReplyWithError(error.get());
    return;
  }
  cryptohome::BaseReply reply;
  brillo::SecureBlob signature;
  if (!boot_lockbox_->Verify(brillo::SecureBlob(request_pb.data()),
                             brillo::SecureBlob(request_pb.signature()))) {
    reply.set_error(cryptohome::CRYPTOHOME_ERROR_LOCKBOX_SIGNATURE_INVALID);
  } else {
    reply.MutableExtension(
        cryptohome::SignBootLockboxReply::reply)->set_signature(
            signature.to_string());
  }
  response->Return(ReplyToVector(reply));
}

void BootLockboxDBusAdaptor::FinalizeBootLockbox(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<
      std::vector<uint8_t>>> response,
    const std::vector<uint8_t>& in_request) {
  cryptohome::FinalizeBootLockboxRequest request_pb;
  if (!request_pb.ParseFromArray(in_request.data(), in_request.size())) {
    brillo::ErrorPtr error = CreateError(DBUS_ERROR_INVALID_ARGS,
        "FinalizeBootLockbox has invalid argument(s).");
    response->ReplyWithError(error.get());
    return;
  }
  cryptohome::BaseReply reply;
  if (!boot_lockbox_->FinalizeBoot()) {
    reply.set_error(cryptohome::CRYPTOHOME_ERROR_TPM_COMM_ERROR);
  }
  response->Return(ReplyToVector(reply));
}

}  // namespace cryptohome

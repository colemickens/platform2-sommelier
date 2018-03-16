// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/bootlockbox/boot_lockbox_client.h"

#include <utility>
#include <vector>

#include <base/timer/elapsed_timer.h>
#include <brillo/glib/object.h>
#include <dbus/cryptohome/dbus-constants.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <cryptohome/proto_bindings/rpc.pb.h>

#include "bootlockbox/dbus-proxies.h"

namespace cryptohome {

std::unique_ptr<BootLockboxClient>
BootLockboxClient::CreateBootLockboxClient() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  if (!bus->Connect()) {
    LOG(ERROR) << "D-Bus system bus is not ready";
    return nullptr;
  }

  auto bootlockbox_proxy =
      std::make_unique<org::chromium::BootLockboxInterfaceProxy>(bus);

  return std::unique_ptr<BootLockboxClient>(
      new BootLockboxClient(std::move(bootlockbox_proxy), bus));
}

BootLockboxClient::BootLockboxClient(
    std::unique_ptr<org::chromium::BootLockboxInterfaceProxy> bootlockbox,
    scoped_refptr<dbus::Bus> bus)
    : bootlockbox_(std::move(bootlockbox)), bus_(bus) {}

BootLockboxClient::~BootLockboxClient() {
  bus_->ShutdownAndBlock();
}

bool BootLockboxClient::Sign(const std::string& digest,
                             std::string* signature_out) {
  base::ElapsedTimer timer;
  cryptohome::SignBootLockboxRequest request;
  request.set_data(digest);
  std::vector<uint8_t> request_array(request.ByteSize());
  CHECK(request.SerializeToArray(request_array.data(), request_array.size()));

  std::vector<uint8_t> reply_array;
  brillo::ErrorPtr error;
  if (!bootlockbox_->SignBootLockbox(request_array, &reply_array, &error)) {
    LOG(ERROR) << "Failed to call SignBootLockbox, error: "
               << error->GetMessage();
    return false;
  }

  cryptohome::BaseReply base_reply;
  if (!base_reply.ParseFromArray(reply_array.data(), reply_array.size())) {
    LOG(ERROR) << "Failed to parse SignBootLockboxReply";
    return false;
  }

  if (base_reply.has_error()) {
    LOG(ERROR) << "Failed to call Sign, error code: " << base_reply.error();
    return false;
  }

  if (!base_reply.HasExtension(cryptohome::SignBootLockboxReply::reply)) {
    LOG(ERROR) << "Missing reply in SignBootLockboxReply";
    return false;
  }

  cryptohome::SignBootLockboxReply signature_reply =
      base_reply.GetExtension(cryptohome::SignBootLockboxReply::reply);
  if (!signature_reply.has_signature()) {
    LOG(ERROR) << "Missing signature in SignBootLockboxReply";
    return false;
  }
  *signature_out = signature_reply.signature();
  LOG(INFO) << "BootLockboxClient::Sign took "
            << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  return true;
}

bool BootLockboxClient::Verify(const std::string& digest,
                               const std::string& signature) {
  base::ElapsedTimer timer;
  cryptohome::VerifyBootLockboxRequest request;
  request.set_data(digest);
  request.set_signature(signature);
  std::vector<uint8_t> request_array(request.ByteSize());
  CHECK(request.SerializeToArray(request_array.data(), request_array.size()));

  std::vector<uint8_t> reply_array;
  brillo::ErrorPtr error;
  if (!bootlockbox_->VerifyBootLockbox(request_array, &reply_array, &error)) {
    LOG(ERROR) << "Failed to call VerifyBootLockbox, " << error->GetMessage();
    return false;
  }

  cryptohome::BaseReply base_reply;
  if (!base_reply.ParseFromArray(reply_array.data(), reply_array.size())) {
    LOG(ERROR) << "Failed to parse VerifyBootLockbox reply message";
    return false;
  }

  if (base_reply.has_error()) {
    LOG(ERROR) << "Error calling VerifyBootLockbox, error code:"
               << base_reply.error();
    return false;
  }
  LOG(INFO) << "BootLockboxClient::Verify took " <<
            timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  return true;
}

bool BootLockboxClient::Finalize() {
  base::ElapsedTimer timer;
  cryptohome::FinalizeBootLockboxRequest request;
  std::vector<uint8_t> request_array(request.ByteSize());
  CHECK(request.SerializeToArray(request_array.data(), request_array.size()));

  std::vector<uint8_t> reply_array;
  brillo::ErrorPtr error;
  if (!bootlockbox_->FinalizeBootLockbox(request_array, &reply_array, &error)) {
    LOG(ERROR) << "Failed to call FinalizeBootLockbox";
    return false;
  }

  cryptohome::BaseReply base_reply;
  if (!base_reply.ParseFromArray(reply_array.data(), reply_array.size())) {
    LOG(ERROR) << "Failed to parse FinalizeBootLockbox reply";
    return false;
  }

  if (base_reply.has_error()) {
    LOG(ERROR) << "Error calling FinalizeBootLockbox, error code: "
               << base_reply.error();
    return false;
  }
  LOG(INFO) << "Finalize took " << timer.Elapsed().InMillisecondsRoundedUp()
            << "ms";
  return true;
}

}  // namespace cryptohome

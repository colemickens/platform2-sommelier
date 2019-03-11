// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "arc/setup/boot_lockbox_client.h"

#include <utility>
#include <vector>

#include <base/timer/elapsed_timer.h>
#include <cryptohome/proto_bindings/rpc.pb.h>
#include <cryptohome-client/cryptohome/dbus-proxies.h>
#include <dbus/cryptohome/dbus-constants.h>
#include <dbus/dbus.h>

namespace arc {

std::unique_ptr<BootLockboxClient>
BootLockboxClient::CreateBootLockboxClient() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);
  if (!bus->Connect()) {
    LOG(ERROR) << "D-Bus system bus is not ready";
    return nullptr;
  }

  auto cryptohome_proxy =
      std::make_unique<org::chromium::CryptohomeInterfaceProxy>(bus);

  return std::unique_ptr<BootLockboxClient>(
      new BootLockboxClient(std::move(cryptohome_proxy), bus));
}

BootLockboxClient::BootLockboxClient(
    std::unique_ptr<org::chromium::CryptohomeInterfaceProxyInterface>
        cryptohome,
    scoped_refptr<dbus::Bus> bus)
    : cryptohome_(std::move(cryptohome)), bus_(bus) {}

BootLockboxClient::~BootLockboxClient() {
  bus_->ShutdownAndBlock();
}

bool BootLockboxClient::IsServiceReady() {
  std::string owner = bus_->GetServiceOwnerAndBlock(
      cryptohome::kCryptohomeServiceName, dbus::Bus::SUPPRESS_ERRORS);
  return !owner.empty();
}

bool BootLockboxClient::IsTpmReady() {
  bool is_tpm_ready = false;
  brillo::ErrorPtr error;
  // Return false if call fails.
  if (!cryptohome_->TpmIsReady(&is_tpm_ready, &error)) {
    LOG(ERROR) << "Failed to call TpmIsReady, error: " << error->GetMessage();
    return false;
  }
  LOG(INFO) << "Is TPM ready: " << is_tpm_ready;
  return is_tpm_ready;
}

bool BootLockboxClient::Sign(const std::string& digest,
                             std::string* signature_out) {
  base::ElapsedTimer timer;
  cryptohome::SignBootLockboxRequest request;
  request.set_data(digest);

  cryptohome::BaseReply base_reply;
  brillo::ErrorPtr error;
  if (!cryptohome_->SignBootLockbox(request, &base_reply, &error)) {
    LOG(ERROR) << "Failed to call SignBootLockbox, error: "
               << error->GetMessage();
    return false;
  }

  if (base_reply.has_error()) {
    LOG(ERROR) << "Failed to call Sign, error code: " << base_reply.error();
    return false;
  }

  if (!base_reply.HasExtension(cryptohome::SignBootLockboxReply::reply)) {
    LOG(ERROR) << "Missing reply field in SignBootLockboxReply";
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

  cryptohome::BaseReply base_reply;
  brillo::ErrorPtr error;
  if (!cryptohome_->VerifyBootLockbox(request, &base_reply, &error)) {
    LOG(ERROR) << "Failed to call VerifyBootLockbox, " << error->GetMessage();
    return false;
  }

  if (base_reply.has_error()) {
    LOG(ERROR) << "Error calling VerifyBootLockbox, error code:"
               << base_reply.error();
    return false;
  }
  LOG(INFO) << "Verifing took " << timer.Elapsed().InMillisecondsRoundedUp()
            << "ms";
  return true;
}

bool BootLockboxClient::Finalize() {
  base::ElapsedTimer timer;
  cryptohome::FinalizeBootLockboxRequest request;

  cryptohome::BaseReply base_reply;
  brillo::ErrorPtr error;
  if (!cryptohome_->FinalizeBootLockbox(request, &base_reply, &error)) {
    LOG(ERROR) << "Failed to call FinalizeBootLockbox";
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

}  // namespace arc

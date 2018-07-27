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

#include "bootlockbox/dbus-proxies.h"

#include "boot_lockbox_rpc.pb.h"  // NOLINT(build/include)

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

BootLockboxClient::~BootLockboxClient() {}

bool BootLockboxClient::Store(const std::string& key,
                              const std::string& digest) {
  base::ElapsedTimer timer;
  cryptohome::StoreBootLockboxRequest request;
  request.set_key(key);
  request.set_data(digest);
  std::vector<uint8_t> request_array(request.ByteSize());
  CHECK(request.SerializeToArray(request_array.data(), request_array.size()));

  std::vector<uint8_t> reply_array;
  brillo::ErrorPtr error;
  if (!bootlockbox_->StoreBootLockbox(request_array, &reply_array, &error)) {
    LOG(ERROR) << "Failed to call StoreBootLockbox, error: "
               << error->GetMessage();
    return false;
  }

  cryptohome::BootLockboxBaseReply reply;
  if (!reply.ParseFromArray(reply_array.data(), reply_array.size())) {
    LOG(ERROR) << "Failed to parse StoreBootLockbox Reply";
    return false;
  }
  if (reply.has_error()) {
    LOG(ERROR) << "Failed to call Store, error code: " << reply.error();
    return false;
  }

  LOG(INFO) << "BootLockboxClient::Store took "
            << timer.Elapsed().InMillisecondsRoundedUp() << "ms";
  return true;
}

bool BootLockboxClient::Read(const std::string& key,
                             std::string* digest) {
  base::ElapsedTimer timer;
  cryptohome::ReadBootLockboxRequest request;
  request.set_key(key);
  std::vector<uint8_t> request_array(request.ByteSize());
  CHECK(request.SerializeToArray(request_array.data(), request_array.size()));

  std::vector<uint8_t> reply_array;
  brillo::ErrorPtr error;
  if (!bootlockbox_->ReadBootLockbox(request_array, &reply_array, &error)) {
    LOG(ERROR) << "Failed to call ReadBootLockbox, error: "
               << error->GetMessage();
    return false;
  }

  cryptohome::BootLockboxBaseReply base_reply;
  if (!base_reply.ParseFromArray(reply_array.data(), reply_array.size())) {
    LOG(ERROR) << "Failed to parse ReadBootLockbox reply";
    return false;
  }

  if (base_reply.has_error()) {
    LOG(ERROR) << "Failed to call ReadBootLockbox, error code: "
               << base_reply.error();
    return false;
  }
  if (!base_reply.HasExtension(cryptohome::ReadBootLockboxReply::reply)) {
    LOG(ERROR) << "Missing reply in ReadBootLockboxReply";
    return false;
  }
  cryptohome::ReadBootLockboxReply read_reply =
      base_reply.GetExtension(cryptohome::ReadBootLockboxReply::reply);
  if (!read_reply.has_data()) {
    LOG(ERROR) << "Missing data field in ReadBootLockboxReply";
    return false;
  }
  *digest = read_reply.data();
  return true;
}

bool BootLockboxClient::Finalize() {
  base::ElapsedTimer timer;
  cryptohome::FinalizeNVRamBootLockboxRequest request;
  std::vector<uint8_t> request_array(request.ByteSize());
  CHECK(request.SerializeToArray(request_array.data(), request_array.size()));

  std::vector<uint8_t> reply_array;
  brillo::ErrorPtr error;
  if (!bootlockbox_->FinalizeBootLockbox(request_array, &reply_array, &error)) {
    LOG(ERROR) << "Failed to call FinalizeBootLockbox";
    return false;
  }

  cryptohome::BootLockboxBaseReply base_reply;
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

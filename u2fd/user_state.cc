// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2fd/user_state.h"

#include <sys/random.h>

#include <base/bind.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/stringprintf.h>
#include <base/sys_byteorder.h>
#include <brillo/file_utils.h>
#include <dbus/login_manager/dbus-constants.h>
#include <openssl/rand.h>

#include "u2fd/util.h"

namespace u2f {

namespace {

constexpr const char kSessionStateStarted[] = "started";
constexpr const char kUserSecretPath[] = "/run/daemon-store/u2f/%s/secret";
constexpr const char kCounterPath[] = "/run/daemon-store/u2f/%s/counter";
constexpr const int kUserSecretSizeBytes = 32;

void OnSignalConnected(const std::string& interface,
                       const std::string& signal,
                       bool success) {
  if (!success) {
    LOG(ERROR) << "Could not connect to signal " << signal << " on interface "
               << interface;
  }
}

}  // namespace

UserState::UserState(scoped_refptr<dbus::Bus> dbus)
    : proxy_(dbus), weak_ptr_factory_(this) {
  proxy_.RegisterSessionStateChangedSignalHandler(
      base::Bind(&UserState::OnSessionStateChanged,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&OnSignalConnected));

  LoadState();
}

base::Optional<brillo::SecureBlob> UserState::GetUserSecret() {
  if (user_secret_.has_value()) {
    return *user_secret_;
  } else {
    LOG(ERROR) << "User secret requested but not available.";
    return base::nullopt;
  }
}

base::Optional<std::vector<uint8_t>> UserState::GetCounter() {
  if (!counter_.has_value()) {
    LOG(ERROR) << "Counter requested but not available.";
    return base::nullopt;
  }

  std::vector<uint8_t> counter_bytes;
  util::AppendToVector(base::HostToNet32(*counter_), &counter_bytes);

  (*counter_)++;

  if (!PersistCounter()) {
    LOG(ERROR) << "Failed to persist updated counter. Attempting to re-load.";
    LoadCounter();
    return base::nullopt;
  }

  return counter_bytes;
}

void UserState::LoadState() {
  UpdatePrimarySessionSanitizedUser();
  if (sanitized_user_.has_value()) {
    LoadOrCreateUserSecret();
    LoadCounter();
  }
}

void UserState::OnSessionStateChanged(const std::string& state) {
  if (state == kSessionStateStarted) {
    LoadState();
  } else {
    sanitized_user_.reset();
    user_secret_.reset();
    counter_.reset();
  }
}

void UserState::UpdatePrimarySessionSanitizedUser() {
  dbus::MethodCall method_call(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerRetrievePrimarySession);
  std::string user;
  std::string sanitized_user;
  brillo::ErrorPtr error;

  if (!proxy_.RetrievePrimarySession(&user, &sanitized_user, &error) ||
      sanitized_user.empty()) {
    LOG(ERROR) << "Failed to retreive current user. This is expected on "
                  "startup if no user is logged in.";
    sanitized_user_.reset();
  } else {
    sanitized_user_ = sanitized_user;
  }
}

void UserState::LoadOrCreateUserSecret() {
  base::FilePath path(
      base::StringPrintf(kUserSecretPath, sanitized_user_->c_str()));

  if (base::PathExists(path)) {
    LoadUserSecret(path);
  } else {
    CreateUserSecret(path);
  }
}

void UserState::LoadUserSecret(const base::FilePath& path) {
  std::string secret_str;
  if (base::ReadFileToString(path, &secret_str) ||
      secret_str.size() != kUserSecretSizeBytes) {
    user_secret_ = brillo::SecureBlob(secret_str);
  } else {
    LOG(ERROR) << "Failed to load user secret from: " << path.value();
    user_secret_.reset();
  }
}

void UserState::CreateUserSecret(const base::FilePath& path) {
  user_secret_ = brillo::SecureBlob(kUserSecretSizeBytes);

  if (RAND_bytes(&user_secret_->front(), user_secret_->size()) != 1) {
    LOG(ERROR) << "Failed to create new user secret";
    user_secret_.reset();
    return;
  }

  if (!brillo::WriteBlobToFileAtomic(path, *user_secret_, 0600)) {
    LOG(INFO) << "Failed to persist new user secret to disk.";
    user_secret_.reset();
    // TODO(louiscollard): Delete file if present? Validate when loading?
  }
}

void UserState::LoadCounter() {
  base::FilePath path(
      base::StringPrintf(kCounterPath, sanitized_user_->c_str()));

  if (!base::PathExists(path)) {
    LOG(INFO) << "U2F counter missing, initializing counter with value of 0.";
    counter_ = 0;
    return;
  }

  std::string counter;
  if (base::ReadFileToString(path, &counter) &&
      counter.size() == sizeof(uint32_t)) {
    // TODO(louiscollard): This, but less gross.
    counter_ = *reinterpret_cast<const uint32_t*>(counter.data());
  } else {
    LOG(ERROR) << "Failed to load counter from: " << path.value();
    counter_.reset();
  }
}

bool UserState::PersistCounter() {
  base::FilePath path(
      base::StringPrintf(kCounterPath, sanitized_user_->c_str()));
  const uint32_t* counter_ptr = &(*counter_);

  return brillo::WriteToFileAtomic(path,
                                   reinterpret_cast<const char*>(counter_ptr),
                                   sizeof(*counter_ptr), 0600);
}

}  // namespace u2f

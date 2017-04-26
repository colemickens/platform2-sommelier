// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/dev_mode_no_owner_restriction.h"

#include <vector>

#include <chromeos/dbus/service_constants.h>
#include <google/protobuf/message_lite.h>

#include "debugd/src/process_with_output.h"

#include "rpc.pb.h"  // NOLINT(build/include)

namespace debugd {

namespace {

// Time in milliseconds to wait for a D-Bus response from cryptohome.
const int kDbusTimeoutMs = 5000;

const char kAccessDeniedErrorString[] =
    "org.chromium.debugd.error.AccessDenied";
const char kDevModeAccessErrorString[] =
    "Use of this tool is restricted to dev mode.";
const char kOwnerAccessErrorString[] =
    "Unavailable after device has an owner or boot lockbox is finalized.";
const char kOwnerQueryErrorString[] =
    "Error encountered when querying D-Bus, cryptohome may be busy.";

// Queries the cryptohome GetLoginStatus D-Bus interface.
//
// Handles lower-level logic for dbus-c++ methods and the cryptohome protobuf
// classes. Cryptohome protobuf responses work by extending the BaseReply class,
// so if an error occurs it's possible to get a reply that does not contain the
// GetLoginStatusReply extension.
//
// |reply| will be filled if a response was received regardless of extension,
// but the function will only return true if reply is filled and has the
// correct GetLoginStatusReply extension.
bool CryptohomeGetLoginStatus(DBus::Connection* system_dbus,
                              cryptohome::BaseReply* reply) {
  cryptohome::GetLoginStatusRequest request;
  DBus::CallMessage msg;
  std::vector<uint8_t> bytes(request.ByteSize(), 0);

  // Set up the message target and protobuf bytes to send.
  if (request.SerializeToArray(bytes.data(), bytes.size()) &&
      msg.destination(cryptohome::kCryptohomeServiceName) &&
      msg.path(cryptohome::kCryptohomeServicePath) &&
      msg.interface(cryptohome::kCryptohomeInterface) &&
      msg.member(cryptohome::kCryptohomeGetLoginStatus)) {
    try {
      DBus::MessageIter write_iter = msg.writer();
      write_iter << bytes;
      // Send the D-Bus message. This can return/throw an error on failure.
      DBus::Message response = system_dbus->send_blocking(msg, kDbusTimeoutMs);
      if (!response.is_error()) {
        const uint8_t* response_bytes;
        size_t response_size =
            response.reader().recurse().get_array(&response_bytes);
        // Return true only if we can parse the reply successfully and it
        // has the proper GetLoginStatusReply extension.
        return reply->ParseFromArray(response_bytes, response_size) &&
               reply->HasExtension(cryptohome::GetLoginStatusReply::reply);
      }
    } catch (...) {
    }
  }
  return false;
}

// Sets a DBus::Error message if the error is non-NULL and hasn't been set yet.
void SetErrorIfNotSet(DBus::Error* error, const char* message) {
  if (error && !error->is_set()) {
    error->set(kAccessDeniedErrorString, message);
  }
}

}  // namespace

DevModeNoOwnerRestriction::DevModeNoOwnerRestriction(
    DBus::Connection* system_dbus)
    : system_dbus_(system_dbus) {
}

bool DevModeNoOwnerRestriction::AllowToolUse(DBus::Error* error) {
  // Check dev mode first to avoid unnecessary cryptohome query delays.
  if (InDevMode()) {
    bool owner_exists, boot_lockbox_finalized;
    if (GetOwnerAndLockboxStatus(&owner_exists, &boot_lockbox_finalized)) {
      if (!(owner_exists || boot_lockbox_finalized)) {
        return true;
      }
      SetErrorIfNotSet(error, kOwnerAccessErrorString);
    }
    // We want to specifically indicate when the query failed  since it may
    // mean that cryptohome is busy and could be tried again later.
    SetErrorIfNotSet(error, kOwnerQueryErrorString);
  }
  SetErrorIfNotSet(error, kDevModeAccessErrorString);
  return false;
}

bool DevModeNoOwnerRestriction::InDevMode() const {
  // The is_developer_end_user script provides a common way to access this
  // information rather than duplicating logic here.
  return ProcessWithOutput::RunProcess("/usr/sbin/is_developer_end_user",
                                       ProcessWithOutput::ArgList{},
                                       true,  // needs root to run properly.
                                       nullptr,  // no stdin.
                                       nullptr,  // no stdout.
                                       nullptr,  // no stderr.
                                       nullptr) == 0;  // no D-Bus error.
}

// Checks for owner user and boot lockbox status.
//
// This function handles the high-level code of checking the cryptohome
// protocol buffer response. Lower-level details of sending the D-Bus function
// and parsing the protocol buffer are handled in CryptohomeGetLoginStatus().
//
// If cryptohome was queried successfully, returns true and |owner_user_exists|
// and |boot_lockbox_finalized| are updated.
bool DevModeNoOwnerRestriction::GetOwnerAndLockboxStatus(
    bool* owner_user_exists,
    bool* boot_lockbox_finalized) {
  cryptohome::BaseReply base_reply;
  if (CryptohomeGetLoginStatus(system_dbus_, &base_reply)) {
    cryptohome::GetLoginStatusReply reply =
        base_reply.GetExtension(cryptohome::GetLoginStatusReply::reply);
    if (reply.has_owner_user_exists() && reply.has_boot_lockbox_finalized()) {
      *owner_user_exists = reply.owner_user_exists();
      *boot_lockbox_finalized = reply.boot_lockbox_finalized();
      return true;
    }
  }
  return false;
}

}  // namespace debugd

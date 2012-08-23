// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ERROR_CONSTANTS_H_
#define CHROMEOS_DBUS_ERROR_CONSTANTS_H_

#include <glib.h>

// To conform to the GError conventions...
#define CHROMEOS_LOGIN_ERROR chromeos_login_error_quark()
GQuark chromeos_login_error_quark();

namespace login_manager {

// Also, conforming to GError conventions
typedef enum {
  CHROMEOS_LOGIN_ERROR_INVALID_EMAIL,   // email address is illegal.
  CHROMEOS_LOGIN_ERROR_EMIT_FAILED,     // could not emit upstart signal.
  CHROMEOS_LOGIN_ERROR_SESSION_EXISTS,  // session already exists for this user.
  CHROMEOS_LOGIN_ERROR_UNKNOWN_PID,     // pid specified is unknown.
  CHROMEOS_LOGIN_ERROR_NO_USER_NSSDB,   // error finding/opening NSS DB.
  CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY,  // attempt to set key is illegal.
  CHROMEOS_LOGIN_ERROR_NO_OWNER_KEY,    // attempt to set prefs before key.
  CHROMEOS_LOGIN_ERROR_VERIFY_FAIL,     // Signature on update request failed.
  CHROMEOS_LOGIN_ERROR_ENCODE_FAIL,     // Encoding signature for writing to
                                        // disk failed.
  CHROMEOS_LOGIN_ERROR_DECODE_FAIL,     // Decoding signature failed.
  CHROMEOS_LOGIN_ERROR_ILLEGAL_USER,    // The user is not on the whitelist.
  CHROMEOS_LOGIN_ERROR_UNKNOWN_PROPERTY,  // No value set for given property.
  CHROMEOS_LOGIN_ERROR_ILLEGAL_SERVICE,  // service is not whitelisted.
  CHROMEOS_LOGIN_ERROR_START_FAIL,      // service start failure.
  CHROMEOS_LOGIN_ERROR_STOP_FAIL,       // service stop failure.
  CHROMEOS_LOGIN_ERROR_ALREADY_SESSION,  // a session has already been started
} ChromeOSLoginError;

}  // namespace login_manager

#endif  // CHROMEOS_DBUS_ERROR_CONSTANTS_H_

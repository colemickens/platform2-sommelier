// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
#define CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

#include <glib.h>

// To conform to the GError conventions...
#define CHROMEOS_LOGIN_ERROR chromeos_login_error_quark()
GQuark chromeos_login_error_quark();

namespace cryptohome {
extern const char *kCryptohomeInterface;
extern const char *kCryptohomeServicePath;
extern const char *kCryptohomeServiceName;
// Methods
extern const char *kCryptohomeCheckKey;
extern const char *kCryptohomeMigrateKey;
extern const char *kCryptohomeRemove;
extern const char *kCryptohomeGetSystemSalt;
extern const char *kCryptohomeIsMounted;
extern const char *kCryptohomeMount;
extern const char *kCryptohomeMountGuest;
extern const char *kCryptohomeUnmount;
extern const char *kCryptohomeTpmIsReady;
extern const char *kCryptohomeTpmIsEnabled;
extern const char *kCryptohomeTpmIsOwned;
extern const char *kCryptohomeTpmIsBeingOwned;
extern const char *kCryptohomeTpmGetPassword;
extern const char *kCryptohomeAsyncCheckKey;
extern const char *kCryptohomeAsyncMigrateKey;
extern const char *kCryptohomeAsyncMount;
extern const char *kCryptohomeAsyncMountGuest;
extern const char *kCryptohomeGetStatusString;
// Signals
extern const char *kSignalAsyncCallStatus;
}  // namespace cryptohome

namespace imageburn {
extern const char *kImageBurnServiceName;
extern const char *kImageBurnServicePath;
extern const char *kImageBurnServiceInterface;
//Methods
extern const char *kBurnImage;
//Signals
extern const char *kSignalBurnFinishedName;
extern const char *kSignalBurnUpdateName;
} // namespace imageburn

namespace login_manager {
extern const char *kSessionManagerInterface;
extern const char *kSessionManagerServicePath;
extern const char *kSessionManagerServiceName;
// Methods
extern const char *kSessionManagerEmitLoginPromptReady;
extern const char *kSessionManagerStartSession;
extern const char *kSessionManagerStopSession;
extern const char *kSessionManagerRestartJob;
extern const char *kSessionManagerSetOwnerKey;
extern const char *kSessionManagerUnwhitelist;
extern const char *kSessionManagerCheckWhitelist;
extern const char *kSessionManagerEnumerateWhitelisted;
extern const char *kSessionManagerWhitelist;
extern const char *kSessionManagerStoreProperty;
extern const char *kSessionManagerRetrieveProperty;
// Signals
extern const char *kSessionManagerSessionStateChanged;

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
  CHROMEOS_LOGIN_ERROR_UNKNOWN_PROPERTY,// No value set for given property.
} ChromeOSLoginError;

}  // namespace login_manager

namespace speech_synthesis {
extern const char *kSpeechSynthesizerInterface;
extern const char *kSpeechSynthesizerServicePath;
extern const char *kSpeechSynthesizerServiceName;
}  // namespace speech_synthesis

namespace chromium {
extern const char* kChromiumInterface;
// ScreenLock signals.
extern const char* kLockScreenSignal;
extern const char* kUnlockScreenSignal;
extern const char* kUnlockScreenFailedSignal;
// Text-to-speech service signals
extern const char* kTTSReadySignal;
extern const char* kTTSFailedSignal;
// Ownership API signals
extern const char* kOwnerKeySetSignal;
extern const char* kPropertyChangeCompleteSignal;
extern const char* kWhitelistChangeCompleteSignal;
}  // namespace chromium

namespace power_manager {
extern const char* kPowerManagerInterface;
extern const char* kRequestLockScreenSignal;
extern const char* kRequestSuspendSignal;
extern const char* kRequestShutdownSignal;
extern const char* kRequestUnlockScreenSignal;
extern const char* kScreenIsLockedSignal;
extern const char* kScreenIsUnlockedSignal;
}  // namespace power_manager

#endif  // CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

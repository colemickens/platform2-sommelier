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
extern const char *kCryptohomeTpmGetPassword;
}  // namespace cryptohome

namespace login_manager {
extern const char *kSessionManagerInterface;
extern const char *kSessionManagerServicePath;
extern const char *kSessionManagerServiceName;
// Methods
extern const char *kSessionManagerEmitLoginPromptReady;
extern const char *kSessionManagerStartSession;
extern const char *kSessionManagerStopSession;
extern const char *kSessionManagerTpmIsReady;
extern const char *kSessionManagerTpmIsEnabled;
extern const char *kSessionManagerGetTpmPassword;
// Signals
extern const char *kSessionManagerSessionStateChanged;

// Also, conforming to GError conventions
typedef enum {
  CHROMEOS_LOGIN_ERROR_INVALID_EMAIL,  // email address is illegal.
  CHROMEOS_LOGIN_ERROR_EMIT_FAILED,    // could not emit upstart signal.
  CHROMEOS_LOGIN_ERROR_SESSION_EXISTS  // session already exists for this user.
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
}  // namespace chromium

namespace power_manager {
extern const char* kPowerManagerInterface;
extern const char* kRequestLockScreenSignal;
extern const char* kRequestSuspendSignal;
extern const char* kRequestUnlockScreenSignal;
extern const char* kScreenIsLockedSignal;
extern const char* kScreenIsUnlockedSignal;
}  // namespace power_manager

#endif  // CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service_constants.h"

GQuark chromeos_login_error_quark() {
  return g_quark_from_static_string("chromeos-login-error-quark");
}

namespace cryptohome {
const char *kCryptohomeInterface = "org.chromium.CryptohomeInterface";
const char *kCryptohomeServiceName = "org.chromium.Cryptohome";
const char *kCryptohomeServicePath = "/org/chromium/Cryptohome";
// Methods
const char *kCryptohomeCheckKey = "CheckKey";
const char *kCryptohomeMigrateKey = "MigrateKey";
const char *kCryptohomeRemove = "Remove";
const char *kCryptohomeGetSystemSalt = "GetSystemSalt";
const char *kCryptohomeIsMounted = "IsMounted";
const char *kCryptohomeMount = "Mount";
const char *kCryptohomeMountGuest = "MountGuest";
const char *kCryptohomeUnmount = "Unmount";
const char *kCryptohomeTpmIsReady = "TpmIsReady";
const char *kCryptohomeTpmIsEnabled = "TpmIsEnabled";
const char *kCryptohomeTpmGetPassword = "TpmGetPassword";
}  // namespace cryptohome

namespace login_manager {
const char *kSessionManagerInterface = "org.chromium.SessionManagerInterface";
const char *kSessionManagerServiceName = "org.chromium.SessionManager";
const char *kSessionManagerServicePath = "/org/chromium/SessionManager";
// Methods
const char *kSessionManagerEmitLoginPromptReady = "EmitLoginPromptReady";
const char *kSessionManagerStartSession = "StartSession";
const char *kSessionManagerStopSession = "StopSession";
const char *kSessionManagerTpmIsReady = "TpmIsReady";
const char *kSessionManagerTpmIsEnabled = "TpmIsEnabled";
const char *kSessionManagerGetTpmPassword = "GetTpmPassword";
// Signals
const char *kSessionManagerSessionStateChanged = "SessionStateChanged";
}  // namespace login_manager

namespace speech_synthesis {
const char *kSpeechSynthesizerInterface =
    "org.chromium.SpeechSynthesizerInterface";
const char *kSpeechSynthesizerServicePath = "/org/chromium/SpeechSynthesizer";
const char *kSpeechSynthesizerServiceName = "org.chromium.SpeechSynthesizer";
}  // namespace speech_synthesis

namespace chromium {
const char* kChromiumInterface = "org.chromium.Chromium";
const char* kLockScreenSignal = "LockScreen";
const char* kUnlockScreenSignal = "UnlockScreen";
const char* kUnlockScreenFailedSignal = "UnlockScreenFailed";
const char* kTTSReadySignal = "TTSReady";
const char* kTTSFailedSignal = "TTSFailed";
}  // namespace chromium

namespace power_manager {
const char* kPowerManagerInterface = "org.chromium.PowerManager";
const char* kRequestLockScreenSignal = "RequestLockScreen";
const char* kRequestSuspendSignal = "RequestSuspend";
const char* kRequestUnlockScreenSignal = "RequestUnlockScreen";
const char* kScreenIsLockedSignal = "ScreenIsLocked";
const char* kScreenIsUnlockedSignal = "ScreenIsUnlocked";
}  // namespace screen_lock

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
const char *kCryptohomeTpmIsOwned = "TpmIsOwned";
const char *kCryptohomeTpmIsBeingOwned = "TpmIsBeingOwned";
const char *kCryptohomeTpmGetPassword = "TpmGetPassword";
const char *kCryptohomeTpmCanAttemptOwnership = "TpmCanAttemptOwnership";
const char *kCryptohomeTpmClearStoredPassword = "TpmClearStoredPassword";
const char *kCryptohomePkcs11GetTpmTokenInfo = "Pkcs11GetTpmTokenInfo";
const char *kCryptohomePkcs11IsTpmTokenReady = "Pkcs11IsTpmTokenReady";
const char *kCryptohomeAsyncCheckKey = "AsyncCheckKey";
const char *kCryptohomeAsyncMigrateKey = "AsyncMigrateKey";
const char *kCryptohomeAsyncMount = "AsyncMount";
const char *kCryptohomeAsyncMountGuest = "AsyncMountGuest";
const char *kCryptohomeAsyncRemove = "AsyncRemove";
const char *kCryptohomeGetStatusString = "GetStatusString";
const char *kCryptohomeRemoveTrackedSubdirectories =
    "RemoveTrackedSubdirectories";
const char *kCryptohomeAsyncRemoveTrackedSubdirectories =
    "AsyncRemoveTrackedSubdirectories";
const char *kCryptohomeDoAutomaticFreeDiskSpaceControl =
    "DoAutomaticFreeDiskSpaceControl";
const char *kCryptohomeAsyncDoAutomaticFreeDiskSpaceControl =
    "AsyncDoAutomaticFreeDiskSpaceControl";
const char *kCryptohomeInstallAttributesGet = "InstallAttributesGet";
const char *kCryptohomeInstallAttributesSet = "InstallAttributesSet";
const char *kCryptohomeInstallAttributesCount = "InstallAttributesCount";
const char *kCryptohomeInstallAttributesFinalize = "InstallAttributesFinalize";
const char *kCryptohomeInstallAttributesIsReady = "InstallAttributesIsReady";
const char *kCryptohomeInstallAttributesIsSecure = "InstallAttributesIsSecure";
const char *kCryptohomeInstallAttributesIsInvalid =
    "InstallAttributesIsInvalid";
const char *kCryptohomeInstallAttributesIsFirstInstall =
    "InstallAttributesIsFirstInstall";

// Signals
const char *kSignalAsyncCallStatus = "AsyncCallStatus";
const char *kSignalTpmInitStatus = "TpmInitStatus";
}  // namespace cryptohome

namespace imageburn {
const char *kImageBurnServiceName = "org.chromium.ImageBurner";
const char *kImageBurnServicePath = "/org/chromium/ImageBurner";
const char *kImageBurnServiceInterface = "org.chromium.ImageBurnerInterface";
//Methods
const char *kBurnImage = "BurnImage";
//Signals
const char *kSignalBurnFinishedName = "burn_finished";
const char *kSignalBurnUpdateName = "burn_progress_update";
} // namespace imageburn

namespace login_manager {
const char *kSessionManagerInterface = "org.chromium.SessionManagerInterface";
const char *kSessionManagerServiceName = "org.chromium.SessionManager";
const char *kSessionManagerServicePath = "/org/chromium/SessionManager";
// Methods
const char *kSessionManagerEmitLoginPromptReady = "EmitLoginPromptReady";
const char *kSessionManagerEmitLoginPromptVisible = "EmitLoginPromptVisible";
const char *kSessionManagerStartSession = "StartSession";
const char *kSessionManagerStopSession = "StopSession";
const char *kSessionManagerRestartJob = "RestartJob";
const char *kSessionManagerRestartEntd = "RestartEntd";
const char *kSessionManagerSetOwnerKey = "SetOwnerKey";
const char *kSessionManagerUnwhitelist = "Unwhitelist";
const char *kSessionManagerCheckWhitelist = "CheckWhitelist";
const char *kSessionManagerEnumerateWhitelisted = "EnumerateWhitelisted";
const char *kSessionManagerWhitelist = "Whitelist";
const char *kSessionManagerStoreProperty = "StoreProperty";
const char *kSessionManagerRetrieveProperty = "RetrieveProperty";
const char *kSessionManagerStorePolicy = "StorePolicy";
const char *kSessionManagerRetrievePolicy = "RetrievePolicy";
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
const char* kOwnerKeySetSignal = "SetOwnerKeyComplete";
const char* kPropertyChangeCompleteSignal = "PropertyChangeComplete";
const char* kWhitelistChangeCompleteSignal = "WhitelistChangeComplete";
}  // namespace chromium

namespace power_manager {
const char* kPowerManagerInterface = "org.chromium.PowerManager";
const char* kRequestLockScreenSignal = "RequestLockScreen";
const char* kRequestRestartSignal = "RequestRestart";
const char* kRequestSuspendSignal = "RequestSuspend";
const char* kRequestShutdownSignal = "RequestShutdown";
const char* kRequestUnlockScreenSignal = "RequestUnlockScreen";
const char* kScreenIsLockedSignal = "ScreenIsLocked";
const char* kScreenIsUnlockedSignal = "ScreenIsUnlocked";
const char* kCleanShutdown = "CleanShutdown";
const char* kRegisterSuspendDelay = "RegisterSuspendDelay";
const char* kUnregisterSuspendDelay = "UnregisterSuspendDelay";
const char* kSuspendDelay = "SuspendDelay";
const char* kSuspendReady = "SuspendReady";
const char* kBrightnessChangedSignal = "BrightnessChanged";
const char* kPowerStateChangedSignal = "PowerStateChanged";
}  // namespace power_manager

namespace chromeos {
const char *kLibCrosServiceName = "org.chromium.LibCrosService";
const char *kLibCrosServicePath = "/org/chromium/LibCrosService";
const char *kLibCrosServiceInterface =
    "org.chromium.LibCrosServiceInterface";
// Methods
const char *kResolveNetworkProxy = "ResolveNetworkProxy";
} // namespace chromeos

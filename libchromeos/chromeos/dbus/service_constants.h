// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
#define CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

#include <glib.h>

// To conform to the GError conventions...
#define CHROMEOS_LOGIN_ERROR chromeos_login_error_quark()
GQuark chromeos_login_error_quark();

namespace cryptohome {
extern const char* kCryptohomeInterface;
extern const char* kCryptohomeServicePath;
extern const char* kCryptohomeServiceName;
// Methods
extern const char* kCryptohomeCheckKey;
extern const char* kCryptohomeMigrateKey;
extern const char* kCryptohomeRemove;
extern const char* kCryptohomeGetSystemSalt;
extern const char* kCryptohomeIsMounted;
extern const char* kCryptohomeMount;
extern const char* kCryptohomeMountGuest;
extern const char* kCryptohomeUnmount;
extern const char* kCryptohomeTpmIsReady;
extern const char* kCryptohomeTpmIsEnabled;
extern const char* kCryptohomeTpmIsOwned;
extern const char* kCryptohomeTpmIsBeingOwned;
extern const char* kCryptohomeTpmGetPassword;
extern const char* kCryptohomeTpmCanAttemptOwnership;
extern const char* kCryptohomeTpmClearStoredPassword;
extern const char* kCryptohomePkcs11GetTpmTokenInfo;
extern const char* kCryptohomePkcs11IsTpmTokenReady;
extern const char* kCryptohomeAsyncCheckKey;
extern const char* kCryptohomeAsyncMigrateKey;
extern const char* kCryptohomeAsyncMount;
extern const char* kCryptohomeAsyncMountGuest;
extern const char* kCryptohomeAsyncRemove;
extern const char* kCryptohomeGetStatusString;
extern const char* kCryptohomeRemoveTrackedSubdirectories;
extern const char* kCryptohomeAsyncRemoveTrackedSubdirectories;
extern const char* kCryptohomeDoAutomaticFreeDiskSpaceControl;
extern const char* kCryptohomeAsyncDoAutomaticFreeDiskSpaceControl;
extern const char* kCryptohomeAsyncDoesUsersExist;
extern const char* kCryptohomeAsyncSetOwnerUser;
extern const char* kCryptohomeInstallAttributesGet;
extern const char* kCryptohomeInstallAttributesSet;
extern const char* kCryptohomeInstallAttributesCount;
extern const char* kCryptohomeInstallAttributesFinalize;
extern const char* kCryptohomeInstallAttributesIsReady;
extern const char* kCryptohomeInstallAttributesIsSecure;
extern const char* kCryptohomeInstallAttributesIsInvalid;
extern const char* kCryptohomeInstallAttributesIsFirstInstall;
// Signals
extern const char* kSignalAsyncCallStatus;
extern const char* kSignalTpmInitStatus;
extern const char* kSignalCleanupUsersRemoved;
}  // namespace cryptohome

namespace imageburn {
extern const char* kImageBurnServiceName;
extern const char* kImageBurnServicePath;
extern const char* kImageBurnServiceInterface;
//Methods
extern const char* kBurnImage;
//Signals
extern const char* kSignalBurnFinishedName;
extern const char* kSignalBurnUpdateName;
} // namespace imageburn

namespace login_manager {
extern const char* kSessionManagerInterface;
extern const char* kSessionManagerServicePath;
extern const char* kSessionManagerServiceName;
// Methods
extern const char* kSessionManagerEmitLoginPromptReady;
extern const char* kSessionManagerEmitLoginPromptVisible;
extern const char* kSessionManagerStartSession;
extern const char* kSessionManagerStopSession;
extern const char* kSessionManagerRestartJob;
extern const char* kSessionManagerRestartEntd;
extern const char* kSessionManagerSetOwnerKey;
extern const char* kSessionManagerUnwhitelist;
extern const char* kSessionManagerCheckWhitelist;
extern const char* kSessionManagerEnumerateWhitelisted;
extern const char* kSessionManagerWhitelist;
extern const char* kSessionManagerStoreProperty;
extern const char* kSessionManagerRetrieveProperty;
extern const char* kSessionManagerStorePolicy;
extern const char* kSessionManagerRetrievePolicy;
extern const char* kSessionManagerStoreUserPolicy;
extern const char* kSessionManagerRetrieveUserPolicy;
extern const char* kSessionManagerRetrieveSessionState;
// Signals
extern const char* kSessionManagerSessionStateChanged;

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
extern const char* kSpeechSynthesizerInterface;
extern const char* kSpeechSynthesizerServicePath;
extern const char* kSpeechSynthesizerServiceName;
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
extern const char* kPowerManagerServicePath;
extern const char* kPowerManagerServiceName;
// Methods
extern const char* kPowerManagerDecreaseScreenBrightness;
extern const char* kPowerManagerIncreaseScreenBrightness;
// Signals
extern const char* kRequestLockScreenSignal;
extern const char* kRequestRestartSignal;
extern const char* kRequestSuspendSignal;
extern const char* kRequestShutdownSignal;
extern const char* kRequestUnlockScreenSignal;
extern const char* kScreenIsLockedSignal;
extern const char* kScreenIsUnlockedSignal;
extern const char* kCleanShutdown;
extern const char* kRegisterSuspendDelay;
extern const char* kUnregisterSuspendDelay;
extern const char* kSuspendDelay;
extern const char* kSuspendReady;
extern const char* kBrightnessChangedSignal;
extern const char* kPowerStateChangedSignal;
}  // namespace power_manager

namespace chromeos {
extern const char* kLibCrosServiceName;
extern const char* kLibCrosServicePath;
extern const char* kLibCrosServiceInterface;
// Methods
extern const char* kResolveNetworkProxy;
} // namespace chromeos

namespace flimflam {
// Flimflam D-Bus service identifiers.
extern const char* kFlimflamManagerInterface;
extern const char* kFlimflamServiceInterface;
extern const char* kFlimflamServiceName;
extern const char* kFlimflamIPConfigInterface;
extern const char* kFlimflamDeviceInterface;
extern const char* kFlimflamProfileInterface;
extern const char* kFlimflamNetworkInterface;

// Flimflam function names.
extern const char* kGetPropertiesFunction;
extern const char* kSetPropertyFunction;
extern const char* kClearPropertyFunction;
extern const char* kConnectFunction;
extern const char* kDisconnectFunction;
extern const char* kRequestScanFunction;
extern const char* kGetWifiServiceFunction;
extern const char* kGetVPNServiceFunction;
extern const char* kEnableTechnologyFunction;
extern const char* kDisableTechnologyFunction;
extern const char* kAddIPConfigFunction;
extern const char* kRemoveConfigFunction;
extern const char* kGetEntryFunction;
extern const char* kDeleteEntryFunction;
extern const char* kActivateCellularModemFunction;
extern const char* kRequirePinFunction;
extern const char* kEnterPinFunction;
extern const char* kUnblockPinFunction;
extern const char* kChangePinFunction;
extern const char* kProposeScanFunction;
extern const char* kRegisterFunction;

// Flimflam property names.
extern const char* kSecurityProperty;
extern const char* kPassphraseProperty;
extern const char* kIdentityProperty;
extern const char* kCertPathProperty; // DEPRECATED
extern const char* kOfflineModeProperty;
extern const char* kSignalStrengthProperty;
extern const char* kNameProperty;
extern const char* kTypeProperty;
extern const char* kUnknownString;
extern const char* kAutoConnectProperty;
extern const char* kModeProperty;
extern const char* kActiveProfileProperty;
extern const char* kSSIDProperty;
extern const char* kDevicesProperty;
extern const char* kNetworksProperty;
extern const char* kConnectedProperty;
extern const char* kWifiChannelProperty;
extern const char* kScanIntervalProperty;
extern const char* kPoweredProperty;
extern const char* kHostProperty;
extern const char* kDBusConnectionProperty;
extern const char* kDBusObjectProperty;

// Flimflam device info property names.
extern const char* kIPConfigsProperty;
extern const char* kCertpathSettingsPrefix;

// Flimflam EAP service properties
extern const char* kEAPEAPProperty;
extern const char* kEAPClientCertProperty;
extern const char* kEAPCertIDProperty;
extern const char* kEAPKeyIDProperty;
extern const char* kEAPPINProperty;

// Flimflam VPN service properties
extern const char* kVPNDomainProperty;

// Flimflam monitored properties
extern const char* kMonitorPropertyChanged;

// Flimflam type options.
extern const char* kTypeWifi;

// Flimflam mode options.
extern const char* kModeManaged;

// IPConfig property names.
extern const char* kMethodProperty;
extern const char* kAddressProperty;
extern const char* kMtuProperty;
extern const char* kPrefixlenProperty;
extern const char* kBroadcastProperty;
extern const char* kPeerAddressProperty;
extern const char* kGatewayProperty;
extern const char* kDomainNameProperty;
extern const char* kNameServersProperty;

// IPConfig type options.
extern const char* kTypeIPv4;
extern const char* kTypeIPv6;
extern const char* kTypeDHCP;
extern const char* kTypeBOOTP;
extern const char* kTypeZeroConf;
extern const char* kTypeDHCP6;
extern const char* kTypePPP;
}  // namespace flimflam

namespace cashew {
// Cashew D-Bus service identifiers.
extern const char* kCashewServiceName;
extern const char* kCashewServicePath;
extern const char* kCashewServiceInterface;

// Cashew function names.
extern const char* kRequestDataPlanFunction;
extern const char* kRetrieveDataPlanFunction;

// Cashew signals.
extern const char* kMonitorDataPlanUpdate;

// Cashew data plan properties
extern const char* kCellularPlanNameProperty;
extern const char* kCellularPlanTypeProperty;
extern const char* kCellularPlanUpdateTimeProperty;
extern const char* kCellularPlanStartProperty;
extern const char* kCellularPlanEndProperty;
extern const char* kCellularPlanDataBytesProperty;
extern const char* kCellularDataBytesUsedProperty;

// Cashew Data Plan types
extern const char* kCellularDataPlanUnlimited;
extern const char* kCellularDataPlanMeteredPaid;
extern const char* kCellularDataPlanMeteredBase;
}  // namespace cashew

namespace modemmanager {
// ModemManager D-Bus service identifiers
extern const char* kModemManagerSMSInterface;

// ModemManager function names.
extern const char* kSMSGetFunction;
extern const char* kSMSDeleteFunction;
extern const char* kSMSListFunction;

// ModemManager monitored signals
extern const char* kSMSReceivedSignal;
}  // namespace modemmanager

#endif  // CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

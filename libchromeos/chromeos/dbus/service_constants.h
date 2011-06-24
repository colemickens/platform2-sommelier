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
extern const char* kPowerManagerDecreaseKeyboardBrightness;
extern const char* kPowerManagerIncreaseKeyboardBrightness;
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
extern const char* kKeyboardBrightnessChangedSignal;
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

// Flimflam Service property names.
extern const char* kSecurityProperty;
extern const char* kPriorityProperty;
extern const char* kPassphraseProperty;
extern const char* kIdentityProperty;
extern const char* kAuthorityPathProperty;
extern const char* kPassphraseRequiredProperty;
extern const char* kSaveCredentialsProperty;
extern const char* kSignalStrengthProperty;
extern const char* kNameProperty;
extern const char* kStateProperty;
extern const char* kTypeProperty;
extern const char* kDeviceProperty;
extern const char* kProfileProperty;
extern const char* kConnectivityStateProperty;
extern const char* kFavoriteProperty;
extern const char* kConnectableProperty;
extern const char* kAutoConnectProperty;
extern const char* kIsActiveProperty;
extern const char* kModeProperty;
extern const char* kErrorProperty;
extern const char* kProviderProperty;
extern const char* kHostProperty;
extern const char* kDomainProperty;
extern const char* kProxyConfigProperty;
extern const char* kCheckPortalProperty;
extern const char* kSSIDProperty;
extern const char* kConnectedProperty;

// Flimflam Wifi Service property names.
extern const char* kWifiHexSsid;
extern const char* kWifiFrequency;
extern const char* kWifiHiddenSsid;
extern const char* kWifiPhyMode;
extern const char* kWifiAuthMode;
extern const char* kWifiChannelProperty;

// Flimflam EAP property names.
extern const char* kEapIdentityProperty;
extern const char* kEAPEAPProperty;
extern const char* kEapPhase2AuthProperty;
extern const char* kEapAnonymousIdentityProperty;
extern const char* kEAPClientCertProperty;
extern const char* kEAPCertIDProperty;
extern const char* kEapClientCertNssProperty;
extern const char* kEapPrivateKeyProperty;
extern const char* kEapPrivateKeyPasswordProperty;
extern const char* kEAPKeyIDProperty;
extern const char* kEapCaCertProperty;
extern const char* kEapCaCertIDProperty;
extern const char* kEapCaCertNssProperty;
extern const char* kEapUseSystemCAsProperty;
extern const char* kEAPPINProperty;
extern const char* kEapPasswordProperty;
extern const char* kEapKeyMgmtProperty;

// Flimflam Cellular Service property names.
extern const char* kActivationStateProperty;
extern const char* kNetworkTechnologyProperty;
extern const char* kRoamingStateProperty;
extern const char* kOperatorNameProperty;
extern const char* kOperatorCodeProperty;
extern const char* kServingOperatorProperty;
extern const char* kPaymentURLProperty;
extern const char* kUsageURLProperty;
extern const char* kCellularApnProperty;
extern const char* kCellularLastGoodApnProperty;

// Flimflam Manager property names.
extern const char* kProfilesProperty;
extern const char* kServicesProperty;
extern const char* kServiceWatchListProperty;
extern const char* kAvailableTechnologiesProperty;
extern const char* kEnabledTechnologiesProperty;
extern const char* kConnectedTechnologiesProperty;
extern const char* kDefaultTechnologyProperty;
extern const char* kOfflineModeProperty;
extern const char* kActiveProfileProperty;
extern const char* kDevicesProperty;
extern const char* kCheckPortalListProperty;
extern const char* kCountryProperty;
extern const char* kPortalURLProperty;

// Flimflam Profile property names.
extern const char* kEntriesProperty;

// Flimflam Device property names.
extern const char* kScanningProperty;
extern const char* kPoweredProperty;
extern const char* kNetworksProperty;
extern const char* kScanIntervalProperty;
extern const char* kBgscanMethodProperty;
extern const char* kBgscanShortIntervalProperty;
extern const char* kDBusConnectionProperty;
extern const char* kDBusObjectProperty;
extern const char* kBgscanSignalThresholdProperty;

// Flimflam Cellular Device property names.
extern const char* kCarrierProperty;
extern const char* kCellularAllowRoamingProperty;
extern const char* kHomeProviderProperty;
extern const char* kMeidProperty;
extern const char* kImeiProperty;
extern const char* kImsiProperty;
extern const char* kEsnProperty;
extern const char* kMdnProperty;
extern const char* kMinProperty;
extern const char* kModelIDProperty;
extern const char* kManufacturerProperty;
extern const char* kFirmwareRevisionProperty;
extern const char* kHardwareRevisionProperty;
extern const char* kPRLVersionProperty;
extern const char* kSelectedNetworkProperty;
extern const char* kSupportNetworkScanProperty;
extern const char* kFoundNetworksProperty;
extern const char* kIPConfigsProperty;

// Flimflam state options.
extern const char* kStateIdle;
extern const char* kStateCarrier;
extern const char* kStateAssociation;
extern const char* kStateConfiguration;
extern const char* kStateReady;
extern const char* kStatePortal;
extern const char* kStateOffline;
extern const char* kStateOnline;
extern const char* kStateDisconnect;
extern const char* kStateFailure;
extern const char* kStateActivationFailure;

// Flimflam property names for SIMLock status.
extern const char* kSIMLockStatusProperty;
extern const char* kSIMLockTypeProperty;
extern const char* kSIMLockRetriesLeftProperty;

// Flimflam property names for Cellular.FoundNetworks.
extern const char* kLongNameProperty;
extern const char* kStatusProperty;
extern const char* kShortNameProperty;
extern const char* kTechnologyProperty;

// Flimflam SIMLock status types.
extern const char* kSIMLockPin;
extern const char* kSIMLockPuk;

// APN info property names.
extern const char* kApnProperty;
extern const char* kNetworkIdProperty;
extern const char* kUsernameProperty;
extern const char* kPasswordProperty;

// Operator info property names.
extern const char* kOperatorNameKey;
extern const char* kOperatorCodeKey;
extern const char* kOperatorCountryKey;

// Flimflam network technology options.
extern const char* kNetworkTechnology1Xrtt;
extern const char* kNetworkTechnologyEvdo;
extern const char* kNetworkTechnologyGprs;
extern const char* kNetworkTechnologyEdge;
extern const char* kNetworkTechnologyUmts;
extern const char* kNetworkTechnologyHspa;
extern const char* kNetworkTechnologyHspaPlus;
extern const char* kNetworkTechnologyLte;
extern const char* kNetworkTechnologyLteAdvanced;

// Flimflam roaming state options
extern const char* kRoamingStateHome;
extern const char* kRoamingStateRoaming;
extern const char* kRoamingStateUnknown;

// Flimflam activation state options
extern const char* kActivationStateActivated;
extern const char* kActivationStateActivating;
extern const char* kActivationStateNotActivated;
extern const char* kActivationStatePartiallyActivated;
extern const char* kActivationStateUnknown;

// Flimflam EAP method options.
extern const char* kEapMethodPEAP;
extern const char* kEapMethodTLS;
extern const char* kEapMethodTTLS;
extern const char* kEapMethodLEAP;

// Flimflam EAP phase 2 auth options.
extern const char* kEapPhase2AuthPEAPMD5;
extern const char* kEapPhase2AuthPEAPMSCHAPV2;
extern const char* kEapPhase2AuthTTLSMD5;
extern const char* kEapPhase2AuthTTLSMSCHAPV2;
extern const char* kEapPhase2AuthTTLSMSCHAP;
extern const char* kEapPhase2AuthTTLSPAP;
extern const char* kEapPhase2AuthTTLSCHAP;

// Flimflam VPN provider types.
extern const char* kProviderL2tpIpsec;
extern const char* kProviderOpenVpn;

// Flimflam VPN service properties
extern const char* kVPNDomainProperty;

// Flimflam monitored properties
extern const char* kMonitorPropertyChanged;

// Flimflam type options.
extern const char* kTypeEthernet;
extern const char* kTypeWifi;
extern const char* kTypeWimax;
extern const char* kTypeBluetooth;
extern const char* kTypeCellular;
extern const char* kTypeVPN;

// Flimflam mode options.
extern const char* kModeManaged;
extern const char* kModeAdhoc;

// Flimflam security options.
extern const char* kSecurityWpa;
extern const char* kSecurityWep;
extern const char* kSecurityRsn;
extern const char* kSecurity8021x;
extern const char* kSecurityPsk;
extern const char* kSecurityNone;

// Flimflam L2TPIPsec property names.
extern const char* kL2TPIPSecCACertNSSProperty;
extern const char* kL2TPIPSecClientCertIDProperty;
extern const char* kL2TPIPSecPSKProperty;
extern const char* kL2TPIPSecUserProperty;
extern const char* kL2TPIPSecPasswordProperty;

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

// Flimflam error options.
extern const char* kErrorOutOfRange;
extern const char* kErrorPinMissing;
extern const char* kErrorDhcpFailed;
extern const char* kErrorConnectFailed;
extern const char* kErrorBadPassphrase;
extern const char* kErrorBadWEPKey;
extern const char* kErrorActivationFailed;
extern const char* kErrorNeedEvdo;
extern const char* kErrorNeedHomeNetwork;
extern const char* kErrorOtaspFailed;
extern const char* kErrorAaaFailed;
extern const char* kErrorInternal;
extern const char* kErrorDNSLookupFailed;
extern const char* kErrorHTTPGetFailed;

// Flimflam error messages.
extern const char* kErrorPassphraseRequiredMsg;
extern const char* kErrorIncorrectPinMsg;
extern const char* kErrorPinBlockedMsg;
extern const char* kErrorPinRequiredMsg;

extern const char* kUnknownString;
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

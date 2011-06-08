// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "service_constants.h"

GQuark chromeos_login_error_quark() {
  return g_quark_from_static_string("chromeos-login-error-quark");
}

namespace cryptohome {
const char* kCryptohomeInterface = "org.chromium.CryptohomeInterface";
const char* kCryptohomeServiceName = "org.chromium.Cryptohome";
const char* kCryptohomeServicePath = "/org/chromium/Cryptohome";
// Methods
const char* kCryptohomeCheckKey = "CheckKey";
const char* kCryptohomeMigrateKey = "MigrateKey";
const char* kCryptohomeRemove = "Remove";
const char* kCryptohomeGetSystemSalt = "GetSystemSalt";
const char* kCryptohomeIsMounted = "IsMounted";
const char* kCryptohomeMount = "Mount";
const char* kCryptohomeMountGuest = "MountGuest";
const char* kCryptohomeUnmount = "Unmount";
const char* kCryptohomeTpmIsReady = "TpmIsReady";
const char* kCryptohomeTpmIsEnabled = "TpmIsEnabled";
const char* kCryptohomeTpmIsOwned = "TpmIsOwned";
const char* kCryptohomeTpmIsBeingOwned = "TpmIsBeingOwned";
const char* kCryptohomeTpmGetPassword = "TpmGetPassword";
const char* kCryptohomeTpmCanAttemptOwnership = "TpmCanAttemptOwnership";
const char* kCryptohomeTpmClearStoredPassword = "TpmClearStoredPassword";
const char* kCryptohomePkcs11GetTpmTokenInfo = "Pkcs11GetTpmTokenInfo";
const char* kCryptohomePkcs11IsTpmTokenReady = "Pkcs11IsTpmTokenReady";
const char* kCryptohomeAsyncCheckKey = "AsyncCheckKey";
const char* kCryptohomeAsyncMigrateKey = "AsyncMigrateKey";
const char* kCryptohomeAsyncMount = "AsyncMount";
const char* kCryptohomeAsyncMountGuest = "AsyncMountGuest";
const char* kCryptohomeAsyncRemove = "AsyncRemove";
const char* kCryptohomeGetStatusString = "GetStatusString";
const char* kCryptohomeRemoveTrackedSubdirectories =
    "RemoveTrackedSubdirectories";
const char* kCryptohomeAsyncRemoveTrackedSubdirectories =
    "AsyncRemoveTrackedSubdirectories";
const char* kCryptohomeDoAutomaticFreeDiskSpaceControl =
    "DoAutomaticFreeDiskSpaceControl";
const char* kCryptohomeAsyncDoAutomaticFreeDiskSpaceControl =
    "AsyncDoAutomaticFreeDiskSpaceControl";
const char* kCryptohomeAsyncDoesUsersExist = "AsyncDoesUsersExist";
const char* kCryptohomeAsyncSetOwnerUser = "AsyncSetOwnerUser";
const char* kCryptohomeInstallAttributesGet = "InstallAttributesGet";
const char* kCryptohomeInstallAttributesSet = "InstallAttributesSet";
const char* kCryptohomeInstallAttributesCount = "InstallAttributesCount";
const char* kCryptohomeInstallAttributesFinalize = "InstallAttributesFinalize";
const char* kCryptohomeInstallAttributesIsReady = "InstallAttributesIsReady";
const char* kCryptohomeInstallAttributesIsSecure = "InstallAttributesIsSecure";
const char* kCryptohomeInstallAttributesIsInvalid =
    "InstallAttributesIsInvalid";
const char* kCryptohomeInstallAttributesIsFirstInstall =
    "InstallAttributesIsFirstInstall";

// Signals
const char* kSignalAsyncCallStatus = "AsyncCallStatus";
const char* kSignalTpmInitStatus = "TpmInitStatus";
const char* kSignalCleanupUsersRemoved = "CleanupUsersRemoved";
}  // namespace cryptohome

namespace imageburn {
const char* kImageBurnServiceName = "org.chromium.ImageBurner";
const char* kImageBurnServicePath = "/org/chromium/ImageBurner";
const char* kImageBurnServiceInterface = "org.chromium.ImageBurnerInterface";
//Methods
const char* kBurnImage = "BurnImage";
//Signals
const char* kSignalBurnFinishedName = "burn_finished";
const char* kSignalBurnUpdateName = "burn_progress_update";
} // namespace imageburn

namespace login_manager {
const char* kSessionManagerInterface = "org.chromium.SessionManagerInterface";
const char* kSessionManagerServiceName = "org.chromium.SessionManager";
const char* kSessionManagerServicePath = "/org/chromium/SessionManager";
// Methods
const char* kSessionManagerEmitLoginPromptReady = "EmitLoginPromptReady";
const char* kSessionManagerEmitLoginPromptVisible = "EmitLoginPromptVisible";
const char* kSessionManagerStartSession = "StartSession";
const char* kSessionManagerStopSession = "StopSession";
const char* kSessionManagerRestartJob = "RestartJob";
const char* kSessionManagerRestartEntd = "RestartEntd";
const char* kSessionManagerSetOwnerKey = "SetOwnerKey";
const char* kSessionManagerUnwhitelist = "Unwhitelist";
const char* kSessionManagerCheckWhitelist = "CheckWhitelist";
const char* kSessionManagerEnumerateWhitelisted = "EnumerateWhitelisted";
const char* kSessionManagerWhitelist = "Whitelist";
const char* kSessionManagerStoreProperty = "StoreProperty";
const char* kSessionManagerRetrieveProperty = "RetrieveProperty";
const char* kSessionManagerStorePolicy = "StorePolicy";
const char* kSessionManagerRetrievePolicy = "RetrievePolicy";
const char* kSessionManagerRetrieveSessionState = "RetrieveSessionState";
// Signals
const char* kSessionManagerSessionStateChanged = "SessionStateChanged";
}  // namespace login_manager

namespace speech_synthesis {
const char* kSpeechSynthesizerInterface =
    "org.chromium.SpeechSynthesizerInterface";
const char* kSpeechSynthesizerServicePath = "/org/chromium/SpeechSynthesizer";
const char* kSpeechSynthesizerServiceName = "org.chromium.SpeechSynthesizer";
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
const char* kPowerManagerServicePath = "/org/chromium/PowerManager";
const char* kPowerManagerServiceName = "org.chromium.PowerManager";
// Methods
const char* kPowerManagerDecreaseScreenBrightness = "DecreaseScreenBrightness";
const char* kPowerManagerIncreaseScreenBrightness = "IncreaseScreenBrightness";
// Signals
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
const char* kLibCrosServiceName = "org.chromium.LibCrosService";
const char* kLibCrosServicePath = "/org/chromium/LibCrosService";
const char* kLibCrosServiceInterface =
    "org.chromium.LibCrosServiceInterface";
// Methods
const char* kResolveNetworkProxy = "ResolveNetworkProxy";
} // namespace chromeos


namespace flimflam {
// Flimflam D-Bus service identifiers.
const char* kFlimflamManagerInterface = "org.chromium.flimflam.Manager";
const char* kFlimflamServiceInterface = "org.chromium.flimflam.Service";
const char* kFlimflamServiceName = "org.chromium.flimflam";
const char* kFlimflamIPConfigInterface = "org.chromium.flimflam.IPConfig";
const char* kFlimflamDeviceInterface = "org.chromium.flimflam.Device";
const char* kFlimflamProfileInterface = "org.chromium.flimflam.Profile";
const char* kFlimflamNetworkInterface = "org.chromium.flimflam.Network";

// Flimflam function names.
const char* kGetPropertiesFunction = "GetProperties";
const char* kSetPropertyFunction = "SetProperty";
const char* kClearPropertyFunction = "ClearProperty";
const char* kConnectFunction = "Connect";
const char* kDisconnectFunction = "Disconnect";
const char* kRequestScanFunction = "RequestScan";
const char* kGetWifiServiceFunction = "GetWifiService";
const char* kGetVPNServiceFunction = "GetVPNService";
const char* kEnableTechnologyFunction = "EnableTechnology";
const char* kDisableTechnologyFunction = "DisableTechnology";
const char* kAddIPConfigFunction = "AddIPConfig";
const char* kRemoveConfigFunction = "Remove";
const char* kGetEntryFunction = "GetEntry";
const char* kDeleteEntryFunction = "DeleteEntry";
const char* kActivateCellularModemFunction = "ActivateCellularModem";
const char* kRequirePinFunction = "RequirePin";
const char* kEnterPinFunction = "EnterPin";
const char* kUnblockPinFunction = "UnblockPin";
const char* kChangePinFunction = "ChangePin";
const char* kProposeScanFunction = "ProposeScan";
const char* kRegisterFunction = "Register";

// Flimflam property names.
const char* kSecurityProperty = "Security";
const char* kPassphraseProperty = "Passphrase";
const char* kIdentityProperty = "Identity";
const char* kCertPathProperty = "CertPath"; // DEPRECATED
const char* kOfflineModeProperty = "OfflineMode";
const char* kSignalStrengthProperty = "Strength";
const char* kNameProperty = "Name";
const char* kTypeProperty = "Type";
const char* kUnknownString = "UNKNOWN";
const char* kAutoConnectProperty = "AutoConnect";
const char* kModeProperty = "Mode";
const char* kActiveProfileProperty = "ActiveProfile";
const char* kSSIDProperty = "SSID";
const char* kDevicesProperty = "Devices";
const char* kNetworksProperty = "Networks";
const char* kConnectedProperty = "Connected";
const char* kWifiChannelProperty = "WiFi.Channel";
const char* kScanIntervalProperty = "ScanInterval";
const char* kPoweredProperty = "Powered";
const char* kHostProperty = "Host";
const char* kDBusConnectionProperty = "DBus.Connection";
const char* kDBusObjectProperty = "DBus.Object";

// Flimflam device info property names.
const char* kIPConfigsProperty = "IPConfigs";
const char* kCertpathSettingsPrefix = "SETTINGS:";

// Flimflam EAP service properties
const char* kEAPEAPProperty = "EAP.EAP";
const char* kEAPClientCertProperty = "EAP.ClientCert";
const char* kEAPCertIDProperty = "EAP.CertID";
const char* kEAPKeyIDProperty = "EAP.KeyID";
const char* kEAPPINProperty = "EAP.PIN";

// Flimflam VPN service properties
const char* kVPNDomainProperty = "VPN.Domain";

// Flimflam monitored properties
const char* kMonitorPropertyChanged = "PropertyChanged";

// Flimflam type options.
const char* kTypeWifi = "wifi";

// Flimflam mode options.
const char* kModeManaged = "managed";

// IPConfig property names.
const char* kMethodProperty = "Method";
const char* kAddressProperty = "Address";
const char* kMtuProperty = "Mtu";
const char* kPrefixlenProperty = "Prefixlen";
const char* kBroadcastProperty = "Broadcast";
const char* kPeerAddressProperty = "PeerAddress";
const char* kGatewayProperty = "Gateway";
const char* kDomainNameProperty = "DomainName";
const char* kNameServersProperty = "NameServers";

// IPConfig type options.
const char* kTypeIPv4 = "ipv4";
const char* kTypeIPv6 = "ipv6";
const char* kTypeDHCP = "dhcp";
const char* kTypeBOOTP = "bootp";
const char* kTypeZeroConf = "zeroconf";
const char* kTypeDHCP6 = "dhcp6";
const char* kTypePPP = "ppp";
}  // namespace flimflam

namespace cashew {
// Cashew D-Bus service identifiers.
const char* kCashewServiceName = "org.chromium.Cashew";
const char* kCashewServicePath = "/org/chromium/Cashew";
const char* kCashewServiceInterface = "org.chromium.Cashew";

// Cashew function names.
const char* kRequestDataPlanFunction = "RequestDataPlansUpdate";
const char* kRetrieveDataPlanFunction = "GetDataPlans";

// Cashew properties monitored by flimflam.
const char* kMonitorDataPlanUpdate = "DataPlansUpdate";

// Cashew data plan properties
const char* kCellularPlanNameProperty = "CellularPlanName";
const char* kCellularPlanTypeProperty = "CellularPlanType";
const char* kCellularPlanUpdateTimeProperty = "CellularPlanUpdateTime";
const char* kCellularPlanStartProperty = "CellularPlanStart";
const char* kCellularPlanEndProperty = "CellularPlanEnd";
const char* kCellularPlanDataBytesProperty = "CellularPlanDataBytes";
const char* kCellularDataBytesUsedProperty = "CellularDataBytesUsed";

// Cashew Data Plan types
const char* kCellularDataPlanUnlimited = "UNLIMITED";
const char* kCellularDataPlanMeteredPaid = "METERED_PAID";
const char* kCellularDataPlanMeteredBase = "METERED_BASE";
}  // namespace cashew

namespace modemmanager {
// ModemManager D-Bus service identifiers
const char* kModemManagerSMSInterface =
    "org.freedesktop.ModemManager.Modem.Gsm.SMS";

// ModemManager function names.
const char* kSMSGetFunction = "Get";
const char* kSMSDeleteFunction = "Delete";
const char* kSMSListFunction = "List";

// ModemManager monitored signals
const char* kSMSReceivedSignal = "SmsReceived";
}  // namespace modemmanager

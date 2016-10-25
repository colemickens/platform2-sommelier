// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/policy/policy_keys.h"

namespace policy {

namespace key {

const char kAdditionalLaunchParameters[] = "AdditionalLaunchParameters";
const char kAllowCrossOriginAuthPrompt[] = "AllowCrossOriginAuthPrompt";
const char kAllowDeletingBrowserHistory[] = "AllowDeletingBrowserHistory";
const char kAllowDinosaurEasterEgg[] = "AllowDinosaurEasterEgg";
const char kAllowFileSelectionDialogs[] = "AllowFileSelectionDialogs";
const char kAllowKioskAppControlChromeVersion[] =
    "AllowKioskAppControlChromeVersion";
const char kAllowOutdatedPlugins[] = "AllowOutdatedPlugins";
const char kAllowScreenLock[] = "AllowScreenLock";
const char kAllowScreenWakeLocks[] = "AllowScreenWakeLocks";
const char kAllowedDomainsForApps[] = "AllowedDomainsForApps";
const char kAlternateErrorPagesEnabled[] = "AlternateErrorPagesEnabled";
const char kAlwaysAuthorizePlugins[] = "AlwaysAuthorizePlugins";
const char kAlwaysOpenPdfExternally[] = "AlwaysOpenPdfExternally";
const char kApplicationLocaleValue[] = "ApplicationLocaleValue";
const char kArcBackupRestoreEnabled[] = "ArcBackupRestoreEnabled";
const char kArcCertificatesSyncMode[] = "ArcCertificatesSyncMode";
const char kArcEnabled[] = "ArcEnabled";
const char kArcPolicy[] = "ArcPolicy";
const char kAttestationEnabledForDevice[] = "AttestationEnabledForDevice";
const char kAttestationEnabledForUser[] = "AttestationEnabledForUser";
const char kAttestationExtensionWhitelist[] = "AttestationExtensionWhitelist";
const char kAttestationForContentProtectionEnabled[] =
    "AttestationForContentProtectionEnabled";
const char kAudioCaptureAllowed[] = "AudioCaptureAllowed";
const char kAudioCaptureAllowedUrls[] = "AudioCaptureAllowedUrls";
const char kAudioOutputAllowed[] = "AudioOutputAllowed";
const char kAuthAndroidNegotiateAccountType[] =
    "AuthAndroidNegotiateAccountType";
const char kAuthNegotiateDelegateWhitelist[] = "AuthNegotiateDelegateWhitelist";
const char kAuthSchemes[] = "AuthSchemes";
const char kAuthServerWhitelist[] = "AuthServerWhitelist";
const char kAutoCleanUpStrategy[] = "AutoCleanUpStrategy";
const char kAutoFillEnabled[] = "AutoFillEnabled";
const char kAutoSelectCertificateForUrls[] = "AutoSelectCertificateForUrls";
const char kBackgroundModeEnabled[] = "BackgroundModeEnabled";
const char kBlockThirdPartyCookies[] = "BlockThirdPartyCookies";
const char kBookmarkBarEnabled[] = "BookmarkBarEnabled";
const char kBrowserAddPersonEnabled[] = "BrowserAddPersonEnabled";
const char kBrowserGuestModeEnabled[] = "BrowserGuestModeEnabled";
const char kBuiltInDnsClientEnabled[] = "BuiltInDnsClientEnabled";
const char kCaptivePortalAuthenticationIgnoresProxy[] =
    "CaptivePortalAuthenticationIgnoresProxy";
const char kCertificateTransparencyEnforcementDisabledForUrls[] =
    "CertificateTransparencyEnforcementDisabledForUrls";
const char kChromeFrameContentTypes[] = "ChromeFrameContentTypes";
const char kChromeFrameRendererSettings[] = "ChromeFrameRendererSettings";
const char kChromeOsLockOnIdleSuspend[] = "ChromeOsLockOnIdleSuspend";
const char kChromeOsMultiProfileUserBehavior[] =
    "ChromeOsMultiProfileUserBehavior";
const char kChromeOsReleaseChannel[] = "ChromeOsReleaseChannel";
const char kChromeOsReleaseChannelDelegated[] =
    "ChromeOsReleaseChannelDelegated";
const char kClearSiteDataOnExit[] = "ClearSiteDataOnExit";
const char kCloudPrintProxyEnabled[] = "CloudPrintProxyEnabled";
const char kCloudPrintSubmitEnabled[] = "CloudPrintSubmitEnabled";
const char kComponentUpdatesEnabled[] = "ComponentUpdatesEnabled";
const char kContentPackDefaultFilteringBehavior[] =
    "ContentPackDefaultFilteringBehavior";
const char kContentPackManualBehaviorHosts[] = "ContentPackManualBehaviorHosts";
const char kContentPackManualBehaviorURLs[] = "ContentPackManualBehaviorURLs";
const char kContextualSearchEnabled[] = "ContextualSearchEnabled";
const char kCookiesAllowedForUrls[] = "CookiesAllowedForUrls";
const char kCookiesBlockedForUrls[] = "CookiesBlockedForUrls";
const char kCookiesSessionOnlyForUrls[] = "CookiesSessionOnlyForUrls";
const char kDHEEnabled[] = "DHEEnabled";
const char kDataCompressionProxyEnabled[] = "DataCompressionProxyEnabled";
const char kDefaultBrowserSettingEnabled[] = "DefaultBrowserSettingEnabled";
const char kDefaultCookiesSetting[] = "DefaultCookiesSetting";
const char kDefaultGeolocationSetting[] = "DefaultGeolocationSetting";
const char kDefaultImagesSetting[] = "DefaultImagesSetting";
const char kDefaultJavaScriptSetting[] = "DefaultJavaScriptSetting";
const char kDefaultKeygenSetting[] = "DefaultKeygenSetting";
const char kDefaultMediaStreamSetting[] = "DefaultMediaStreamSetting";
const char kDefaultNotificationsSetting[] = "DefaultNotificationsSetting";
const char kDefaultPluginsSetting[] = "DefaultPluginsSetting";
const char kDefaultPopupsSetting[] = "DefaultPopupsSetting";
const char kDefaultPrinterSelection[] = "DefaultPrinterSelection";
const char kDefaultSearchProviderAlternateURLs[] =
    "DefaultSearchProviderAlternateURLs";
const char kDefaultSearchProviderEnabled[] = "DefaultSearchProviderEnabled";
const char kDefaultSearchProviderEncodings[] = "DefaultSearchProviderEncodings";
const char kDefaultSearchProviderIconURL[] = "DefaultSearchProviderIconURL";
const char kDefaultSearchProviderImageURL[] = "DefaultSearchProviderImageURL";
const char kDefaultSearchProviderImageURLPostParams[] =
    "DefaultSearchProviderImageURLPostParams";
const char kDefaultSearchProviderInstantURL[] =
  "DefaultSearchProviderInstantURL";
const char kDefaultSearchProviderInstantURLPostParams[] =
    "DefaultSearchProviderInstantURLPostParams";
const char kDefaultSearchProviderKeyword[] = "DefaultSearchProviderKeyword";
const char kDefaultSearchProviderName[] = "DefaultSearchProviderName";
const char kDefaultSearchProviderNewTabURL[] = "DefaultSearchProviderNewTabURL";
const char kDefaultSearchProviderSearchTermsReplacementKey[] =
    "DefaultSearchProviderSearchTermsReplacementKey";
const char kDefaultSearchProviderSearchURL[] = "DefaultSearchProviderSearchURL";
const char kDefaultSearchProviderSearchURLPostParams[] =
    "DefaultSearchProviderSearchURLPostParams";
const char kDefaultSearchProviderSuggestURL[] =
    "DefaultSearchProviderSuggestURL";
const char kDefaultSearchProviderSuggestURLPostParams[] =
    "DefaultSearchProviderSuggestURLPostParams";
const char kDefaultWebBluetoothGuardSetting[] =
    "DefaultWebBluetoothGuardSetting";
const char kDeveloperToolsDisabled[] = "DeveloperToolsDisabled";
const char kDeviceAllowBluetooth[] = "DeviceAllowBluetooth";
const char kDeviceAllowNewUsers[] = "DeviceAllowNewUsers";
const char kDeviceAllowRedeemChromeOsRegistrationOffers[] =
    "DeviceAllowRedeemChromeOsRegistrationOffers";
const char kDeviceAppPack[] = "DeviceAppPack";
const char kDeviceAutoUpdateDisabled[] = "DeviceAutoUpdateDisabled";
const char kDeviceAutoUpdateP2PEnabled[] = "DeviceAutoUpdateP2PEnabled";
const char kDeviceBlockDevmode[] = "DeviceBlockDevmode";
const char kDeviceDataRoamingEnabled[] = "DeviceDataRoamingEnabled";
const char kDeviceEphemeralUsersEnabled[] = "DeviceEphemeralUsersEnabled";
const char kDeviceGuestModeEnabled[] = "DeviceGuestModeEnabled";
const char kDeviceIdleLogoutTimeout[] = "DeviceIdleLogoutTimeout";
const char kDeviceIdleLogoutWarningDuration[] =
    "DeviceIdleLogoutWarningDuration";
const char kDeviceLocalAccountAutoLoginBailoutEnabled[] =
    "DeviceLocalAccountAutoLoginBailoutEnabled";
const char kDeviceLocalAccountAutoLoginDelay[] =
    "DeviceLocalAccountAutoLoginDelay";
const char kDeviceLocalAccountAutoLoginId[] = "DeviceLocalAccountAutoLoginId";
const char kDeviceLocalAccountPromptForNetworkWhenOffline[] =
    "DeviceLocalAccountPromptForNetworkWhenOffline";
const char kDeviceLocalAccounts[] = "DeviceLocalAccounts";
const char kDeviceLoginScreenDefaultHighContrastEnabled[] =
    "DeviceLoginScreenDefaultHighContrastEnabled";
const char kDeviceLoginScreenDefaultLargeCursorEnabled[] =
    "DeviceLoginScreenDefaultLargeCursorEnabled";
const char kDeviceLoginScreenDefaultScreenMagnifierType[] =
    "DeviceLoginScreenDefaultScreenMagnifierType";
const char kDeviceLoginScreenDefaultSpokenFeedbackEnabled[] =
    "DeviceLoginScreenDefaultSpokenFeedbackEnabled";
const char kDeviceLoginScreenDefaultVirtualKeyboardEnabled[] =
    "DeviceLoginScreenDefaultVirtualKeyboardEnabled";
const char kDeviceLoginScreenDomainAutoComplete[] =
    "DeviceLoginScreenDomainAutoComplete";
const char kDeviceLoginScreenPowerManagement[] =
    "DeviceLoginScreenPowerManagement";
const char kDeviceLoginScreenSaverId[] = "DeviceLoginScreenSaverId";
const char kDeviceLoginScreenSaverTimeout[] = "DeviceLoginScreenSaverTimeout";
const char kDeviceMetricsReportingEnabled[] = "DeviceMetricsReportingEnabled";
const char kDeviceOpenNetworkConfiguration[] = "DeviceOpenNetworkConfiguration";
const char kDevicePolicyRefreshRate[] = "DevicePolicyRefreshRate";
const char kDeviceQuirksDownloadEnabled[] = "DeviceQuirksDownloadEnabled";
const char kDeviceRebootOnShutdown[] = "DeviceRebootOnShutdown";
const char kDeviceShowUserNamesOnSignin[] = "DeviceShowUserNamesOnSignin";
const char kDeviceStartUpFlags[] = "DeviceStartUpFlags";
const char kDeviceStartUpUrls[] = "DeviceStartUpUrls";
const char kDeviceTargetVersionPrefix[] = "DeviceTargetVersionPrefix";
const char kDeviceTransferSAMLCookies[] = "DeviceTransferSAMLCookies";
const char kDeviceUpdateAllowedConnectionTypes[] =
    "DeviceUpdateAllowedConnectionTypes";
const char kDeviceUpdateHttpDownloadsEnabled[] =
    "DeviceUpdateHttpDownloadsEnabled";
const char kDeviceUpdateScatterFactor[] = "DeviceUpdateScatterFactor";
const char kDeviceUserWhitelist[] = "DeviceUserWhitelist";
const char kDeviceVariationsRestrictParameter[] =
    "DeviceVariationsRestrictParameter";
const char kDisable3DAPIs[] = "Disable3DAPIs";
const char kDisableAuthNegotiateCnameLookup[] =
    "DisableAuthNegotiateCnameLookup";
const char kDisablePluginFinder[] = "DisablePluginFinder";
const char kDisablePrintPreview[] = "DisablePrintPreview";
const char kDisableSSLRecordSplitting[] = "DisableSSLRecordSplitting";
const char kDisableSafeBrowsingProceedAnyway[] =
    "DisableSafeBrowsingProceedAnyway";
const char kDisableScreenshots[] = "DisableScreenshots";
const char kDisableSpdy[] = "DisableSpdy";
const char kDisabledPlugins[] = "DisabledPlugins";
const char kDisabledPluginsExceptions[] = "DisabledPluginsExceptions";
const char kDisabledSchemes[] = "DisabledSchemes";
const char kDiskCacheDir[] = "DiskCacheDir";
const char kDiskCacheSize[] = "DiskCacheSize";
const char kDisplayRotationDefault[] = "DisplayRotationDefault";
const char kDnsPrefetchingEnabled[] = "DnsPrefetchingEnabled";
const char kDownloadDirectory[] = "DownloadDirectory";
const char kDriveDisabled[] = "DriveDisabled";
const char kDriveDisabledOverCellular[] = "DriveDisabledOverCellular";
const char kEasyUnlockAllowed[] = "EasyUnlockAllowed";
const char kEditBookmarksEnabled[] = "EditBookmarksEnabled";
const char kEnableAuthNegotiatePort[] = "EnableAuthNegotiatePort";
const char kEnableDeprecatedWebBasedSignin[] = "EnableDeprecatedWebBasedSignin";
const char kEnableDeprecatedWebPlatformFeatures[] =
    "EnableDeprecatedWebPlatformFeatures";
const char kEnableMediaRouter[] = "EnableMediaRouter";
const char kEnableMemoryInfo[] = "EnableMemoryInfo";
const char kEnableOnlineRevocationChecks[] = "EnableOnlineRevocationChecks";
const char kEnableOriginBoundCerts[] = "EnableOriginBoundCerts";
const char kEnableSha1ForLocalAnchors[] = "EnableSha1ForLocalAnchors";
const char kEnabledPlugins[] = "EnabledPlugins";
const char kEnterpriseWebStoreName[] = "EnterpriseWebStoreName";
const char kEnterpriseWebStoreURL[] = "EnterpriseWebStoreURL";
const char kExtensionAllowedTypes[] = "ExtensionAllowedTypes";
const char kExtensionCacheSize[] = "ExtensionCacheSize";
const char kExtensionInstallBlacklist[] = "ExtensionInstallBlacklist";
const char kExtensionInstallForcelist[] = "ExtensionInstallForcelist";
const char kExtensionInstallSources[] = "ExtensionInstallSources";
const char kExtensionInstallWhitelist[] = "ExtensionInstallWhitelist";
const char kExtensionSettings[] = "ExtensionSettings";
const char kExternalStorageDisabled[] = "ExternalStorageDisabled";
const char kExternalStorageReadOnly[] = "ExternalStorageReadOnly";
const char kForceBrowserSignin[] = "ForceBrowserSignin";
const char kForceEphemeralProfiles[] = "ForceEphemeralProfiles";
const char kForceGoogleSafeSearch[] = "ForceGoogleSafeSearch";
const char kForceMaximizeOnFirstRun[] = "ForceMaximizeOnFirstRun";
const char kForceSafeSearch[] = "ForceSafeSearch";
const char kForceYouTubeRestrict[] = "ForceYouTubeRestrict";
const char kForceYouTubeSafetyMode[] = "ForceYouTubeSafetyMode";
const char kFullscreenAllowed[] = "FullscreenAllowed";
const char kGCFUserDataDir[] = "GCFUserDataDir";
const char kGSSAPILibraryName[] = "GSSAPILibraryName";
const char kHardwareAccelerationModeEnabled[] =
    "HardwareAccelerationModeEnabled";
const char kHeartbeatEnabled[] = "HeartbeatEnabled";
const char kHeartbeatFrequency[] = "HeartbeatFrequency";
const char kHideWebStoreIcon[] = "HideWebStoreIcon";
const char kHideWebStorePromo[] = "HideWebStorePromo";
const char kHighContrastEnabled[] = "HighContrastEnabled";
const char kHomepageIsNewTabPage[] = "HomepageIsNewTabPage";
const char kHomepageLocation[] = "HomepageLocation";
const char kHttp09OnNonDefaultPortsEnabled[] = "Http09OnNonDefaultPortsEnabled";
const char kIdleAction[] = "IdleAction";
const char kIdleActionAC[] = "IdleActionAC";
const char kIdleActionBattery[] = "IdleActionBattery";
const char kIdleDelayAC[] = "IdleDelayAC";
const char kIdleDelayBattery[] = "IdleDelayBattery";
const char kIdleWarningDelayAC[] = "IdleWarningDelayAC";
const char kIdleWarningDelayBattery[] = "IdleWarningDelayBattery";
const char kImagesAllowedForUrls[] = "ImagesAllowedForUrls";
const char kImagesBlockedForUrls[] = "ImagesBlockedForUrls";
const char kImportAutofillFormData[] = "ImportAutofillFormData";
const char kImportBookmarks[] = "ImportBookmarks";
const char kImportHistory[] = "ImportHistory";
const char kImportHomepage[] = "ImportHomepage";
const char kImportSavedPasswords[] = "ImportSavedPasswords";
const char kImportSearchEngine[] = "ImportSearchEngine";
const char kIncognitoEnabled[] = "IncognitoEnabled";
const char kIncognitoModeAvailability[] = "IncognitoModeAvailability";
const char kInstantEnabled[] = "InstantEnabled";
const char kJavaScriptAllowedForUrls[] = "JavaScriptAllowedForUrls";
const char kJavaScriptBlockedForUrls[] = "JavaScriptBlockedForUrls";
const char kJavascriptEnabled[] = "JavascriptEnabled";
const char kKeyPermissions[] = "KeyPermissions";
const char kKeyboardDefaultToFunctionKeys[] = "KeyboardDefaultToFunctionKeys";
const char kKeygenAllowedForUrls[] = "KeygenAllowedForUrls";
const char kKeygenBlockedForUrls[] = "KeygenBlockedForUrls";
const char kLargeCursorEnabled[] = "LargeCursorEnabled";
const char kLidCloseAction[] = "LidCloseAction";
const char kLogUploadEnabled[] = "LogUploadEnabled";
const char kLoginApps[] = "LoginApps";
const char kLoginAuthenticationBehavior[] = "LoginAuthenticationBehavior";
const char kLoginVideoCaptureAllowedUrls[] = "LoginVideoCaptureAllowedUrls";
const char kManagedBookmarks[] = "ManagedBookmarks";
const char kMaxConnectionsPerProxy[] = "MaxConnectionsPerProxy";
const char kMaxInvalidationFetchDelay[] = "MaxInvalidationFetchDelay";
const char kMediaCacheSize[] = "MediaCacheSize";
const char kMetricsReportingEnabled[] = "MetricsReportingEnabled";
const char kNTPContentSuggestionsEnabled[] = "NTPContentSuggestionsEnabled";
const char kNativeMessagingBlacklist[] = "NativeMessagingBlacklist";
const char kNativeMessagingUserLevelHosts[] = "NativeMessagingUserLevelHosts";
const char kNativeMessagingWhitelist[] = "NativeMessagingWhitelist";
const char kNetworkPredictionOptions[] = "NetworkPredictionOptions";
const char kNotificationsAllowedForUrls[] = "NotificationsAllowedForUrls";
const char kNotificationsBlockedForUrls[] = "NotificationsBlockedForUrls";
const char kOpenNetworkConfiguration[] = "OpenNetworkConfiguration";
const char kPacHttpsUrlStrippingEnabled[] = "PacHttpsUrlStrippingEnabled";
const char kPasswordManagerAllowShowPasswords[] =
    "PasswordManagerAllowShowPasswords";
const char kPasswordManagerEnabled[] = "PasswordManagerEnabled";
const char kPinnedLauncherApps[] = "PinnedLauncherApps";
const char kPluginsAllowedForUrls[] = "PluginsAllowedForUrls";
const char kPluginsBlockedForUrls[] = "PluginsBlockedForUrls";
const char kPolicyRefreshRate[] = "PolicyRefreshRate";
const char kPopupsAllowedForUrls[] = "PopupsAllowedForUrls";
const char kPopupsBlockedForUrls[] = "PopupsBlockedForUrls";
const char kPowerManagementIdleSettings[] = "PowerManagementIdleSettings";
const char kPowerManagementUsesAudioActivity[] =
  "PowerManagementUsesAudioActivity";
const char kPowerManagementUsesVideoActivity[] =
    "PowerManagementUsesVideoActivity";
const char kPresentationIdleDelayScale[] = "PresentationIdleDelayScale";
const char kPresentationScreenDimDelayScale[] =
    "PresentationScreenDimDelayScale";
const char kPrintingEnabled[] = "PrintingEnabled";
const char kProxyBypassList[] = "ProxyBypassList";
const char kProxyMode[] = "ProxyMode";
const char kProxyPacUrl[] = "ProxyPacUrl";
const char kProxyServer[] = "ProxyServer";
const char kProxyServerMode[] = "ProxyServerMode";
const char kProxySettings[] = "ProxySettings";
const char kQuicAllowed[] = "QuicAllowed";
const char kRC4Enabled[] = "RC4Enabled";
const char kRebootAfterUpdate[] = "RebootAfterUpdate";
const char kRegisteredProtocolHandlers[] = "RegisteredProtocolHandlers";
const char kRemoteAccessClientFirewallTraversal[] =
    "RemoteAccessClientFirewallTraversal";
const char kRemoteAccessHostAllowClientPairing[] =
    "RemoteAccessHostAllowClientPairing";
const char kRemoteAccessHostAllowGnubbyAuth[] =
    "RemoteAccessHostAllowGnubbyAuth";
const char kRemoteAccessHostAllowRelayedConnection[] =
    "RemoteAccessHostAllowRelayedConnection";
const char kRemoteAccessHostAllowUiAccessForRemoteAssistance[] =
    "RemoteAccessHostAllowUiAccessForRemoteAssistance";
const char kRemoteAccessHostClientDomain[] = "RemoteAccessHostClientDomain";
const char kRemoteAccessHostDebugOverridePolicies[] =
    "RemoteAccessHostDebugOverridePolicies";
const char kRemoteAccessHostDomain[] = "RemoteAccessHostDomain";
const char kRemoteAccessHostFirewallTraversal[] =
    "RemoteAccessHostFirewallTraversal";
const char kRemoteAccessHostMatchUsername[] = "RemoteAccessHostMatchUsername";
const char kRemoteAccessHostRequireCurtain[] = "RemoteAccessHostRequireCurtain";
const char kRemoteAccessHostRequireTwoFactor[] =
    "RemoteAccessHostRequireTwoFactor";
const char kRemoteAccessHostTalkGadgetPrefix[] =
    "RemoteAccessHostTalkGadgetPrefix";
const char kRemoteAccessHostTokenUrl[] = "RemoteAccessHostTokenUrl";
const char kRemoteAccessHostTokenValidationCertificateIssuer[] =
    "RemoteAccessHostTokenValidationCertificateIssuer";
const char kRemoteAccessHostTokenValidationUrl[] =
    "RemoteAccessHostTokenValidationUrl";
const char kRemoteAccessHostUdpPortRange[] = "RemoteAccessHostUdpPortRange";
const char kRenderInChromeFrameList[] = "RenderInChromeFrameList";
const char kRenderInHostList[] = "RenderInHostList";
const char kReportDeviceActivityTimes[] = "ReportDeviceActivityTimes";
const char kReportDeviceBootMode[] = "ReportDeviceBootMode";
const char kReportDeviceHardwareStatus[] = "ReportDeviceHardwareStatus";
const char kReportDeviceLocation[] = "ReportDeviceLocation";
const char kReportDeviceNetworkInterfaces[] = "ReportDeviceNetworkInterfaces";
const char kReportDeviceSessionStatus[] = "ReportDeviceSessionStatus";
const char kReportDeviceUsers[] = "ReportDeviceUsers";
const char kReportDeviceVersionInfo[] = "ReportDeviceVersionInfo";
const char kReportUploadFrequency[] = "ReportUploadFrequency";
const char kRequireOnlineRevocationChecksForLocalAnchors[] =
    "RequireOnlineRevocationChecksForLocalAnchors";
const char kRestoreOnStartup[] = "RestoreOnStartup";
const char kRestoreOnStartupURLs[] = "RestoreOnStartupURLs";
const char kRestrictSigninToPattern[] = "RestrictSigninToPattern";
const char kSAMLOfflineSigninTimeLimit[] = "SAMLOfflineSigninTimeLimit";
const char kSSLErrorOverrideAllowed[] = "SSLErrorOverrideAllowed";
const char kSSLVersionFallbackMin[] = "SSLVersionFallbackMin";
const char kSSLVersionMin[] = "SSLVersionMin";
const char kSafeBrowsingEnabled[] = "SafeBrowsingEnabled";
const char kSafeBrowsingExtendedReportingOptInAllowed[] =
    "SafeBrowsingExtendedReportingOptInAllowed";
const char kSavingBrowserHistoryDisabled[] = "SavingBrowserHistoryDisabled";
const char kScreenDimDelayAC[] = "ScreenDimDelayAC";
const char kScreenDimDelayBattery[] = "ScreenDimDelayBattery";
const char kScreenLockDelayAC[] = "ScreenLockDelayAC";
const char kScreenLockDelayBattery[] = "ScreenLockDelayBattery";
const char kScreenLockDelays[] = "ScreenLockDelays";
const char kScreenMagnifierType[] = "ScreenMagnifierType";
const char kScreenOffDelayAC[] = "ScreenOffDelayAC";
const char kScreenOffDelayBattery[] = "ScreenOffDelayBattery";
const char kSearchSuggestEnabled[] = "SearchSuggestEnabled";
const char kSessionLengthLimit[] = "SessionLengthLimit";
const char kSessionLocales[] = "SessionLocales";
const char kShelfAutoHideBehavior[] = "ShelfAutoHideBehavior";
const char kShowAccessibilityOptionsInSystemTrayMenu[] =
    "ShowAccessibilityOptionsInSystemTrayMenu";
const char kShowAppsShortcutInBookmarkBar[] = "ShowAppsShortcutInBookmarkBar";
const char kShowHomeButton[] = "ShowHomeButton";
const char kShowLogoutButtonInTray[] = "ShowLogoutButtonInTray";
const char kSigninAllowed[] = "SigninAllowed";
const char kSkipMetadataCheck[] = "SkipMetadataCheck";
const char kSpellCheckServiceEnabled[] = "SpellCheckServiceEnabled";
const char kSpokenFeedbackEnabled[] = "SpokenFeedbackEnabled";
const char kSupervisedUserContentProviderEnabled[] =
    "SupervisedUserContentProviderEnabled";
const char kSupervisedUserCreationEnabled[] = "SupervisedUserCreationEnabled";
const char kSupervisedUsersEnabled[] = "SupervisedUsersEnabled";
const char kSuppressChromeFrameTurndownPrompt[] =
    "SuppressChromeFrameTurndownPrompt";
const char kSuppressUnsupportedOSWarning[] = "SuppressUnsupportedOSWarning";
const char kSyncDisabled[] = "SyncDisabled";
const char kSystemTimezone[] = "SystemTimezone";
const char kSystemTimezoneAutomaticDetection[] =
    "SystemTimezoneAutomaticDetection";
const char kSystemUse24HourClock[] = "SystemUse24HourClock";
const char kTaskManagerEndProcessEnabled[] = "TaskManagerEndProcessEnabled";
const char kTermsOfServiceURL[] = "TermsOfServiceURL";
const char kTouchVirtualKeyboardEnabled[] = "TouchVirtualKeyboardEnabled";
const char kTranslateEnabled[] = "TranslateEnabled";
const char kURLBlacklist[] = "URLBlacklist";
const char kURLWhitelist[] = "URLWhitelist";
const char kUnifiedDesktopEnabledByDefault[] = "UnifiedDesktopEnabledByDefault";
const char kUptimeLimit[] = "UptimeLimit";
const char kUsbDetachableWhitelist[] = "UsbDetachableWhitelist";
const char kUserActivityScreenDimDelayScale[] =
    "UserActivityScreenDimDelayScale";
const char kUserAvatarImage[] = "UserAvatarImage";
const char kUserDataDir[] = "UserDataDir";
const char kUserDisplayName[] = "UserDisplayName";
const char kVariationsRestrictParameter[] = "VariationsRestrictParameter";
const char kVideoCaptureAllowed[] = "VideoCaptureAllowed";
const char kVideoCaptureAllowedUrls[] = "VideoCaptureAllowedUrls";
const char kVirtualKeyboardEnabled[] = "VirtualKeyboardEnabled";
const char kWPADQuickCheckEnabled[] = "WPADQuickCheckEnabled";
const char kWaitForInitialUserActivity[] = "WaitForInitialUserActivity";
const char kWallpaperImage[] = "WallpaperImage";
const char kWebRestrictionsAuthority[] = "WebRestrictionsAuthority";
const char kWebRtcUdpPortRange[] = "WebRtcUdpPortRange";
const char kWelcomePageOnOSUpgradeEnabled[] = "WelcomePageOnOSUpgradeEnabled";

}  // namespace key

}  // namespace policy

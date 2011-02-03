#ifndef PLUGIN_GOBI_SDK_WRAPPER_H_
#define PLUGIN_GOBI_SDK_WRAPPER_H_

/*===========================================================================
  FILE:
  gobi_sdk_wrapper.h,
  derived from
  QCWWANCMAPI2k.h
  which has the following copyright notice:

  DESCRIPTION:
  QUALCOMM Wireless WAN Connection Manager API for Gobi 2000

  Copyright (C) 2009 QUALCOMM Incorporated. All rights reserved.
  QUALCOMM Proprietary/GTDR

  All data and information contained in or disclosed by this document is
  confidential and proprietary information of QUALCOMM Incorporated and all
  rights therein are expressly reserved.  By accepting this material the
  recipient agrees that this material and the information contained therein
  is held in confidence and in trust and will not be used, copied, reproduced
  in whole or in part, nor its contents revealed in any manner to others
  without the express written permission of QUALCOMM Incorporated.
  ==========================================================================*/

#include <stdlib.h>
#include <pthread.h>

#include <base/basictypes.h> // for DISALLOW_COPY_AND_ASSIGN
#include <base/scoped_ptr.h>
#include <dbus/dbus.h>  // for DBus::Path &
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <map>
#include <string>
#include <vector>

// Type Definitions
typedef unsigned long      ULONG;
typedef unsigned long long ULONGLONG;
typedef signed char        INT8;
typedef unsigned char      BYTE;
typedef char               CHAR;
typedef unsigned short     WORD;
typedef unsigned short     USHORT;
typedef const char *       LPCSTR;

// Session state callback function
typedef void (* tFNSessionState)(
   ULONG                      state,
   ULONG                      sessionEndReason );

// RX/TX byte counts callback function
typedef void (* tFNByteTotals)(
   ULONGLONG                  totalBytesTX,
   ULONGLONG                  totalBytesRX );

// Dormancy status callback function
typedef void (* tFNDormancyStatus)( ULONG dormancyStatus );

// Mobile IP status callback function
typedef void (* tFNMobileIPStatus)( ULONG mipStatus );

// Activation status callback function
typedef void (* tFNActivationStatus)( ULONG activationStatus );

// Power operating mode callback function
typedef void (* tFNPower)( ULONG operatingMode );

// Serving system data capabilities callback function
typedef void (* tFNDataCapabilities)(
   BYTE                       dataCapsSize,
   BYTE *                     pDataCaps );

// Data bearer technology callback function
typedef void (* tFNDataBearer)( ULONG dataBearer );

// Roaming indicator callback function
typedef void (* tFNRoamingIndicator)( ULONG roaming );

// Signal strength callback function
typedef void (* tFNSignalStrength)(
   INT8                       signalStrength,
   ULONG                      radioInterface );

// RF information callback function
typedef void (* tFNRFInfo)(
   ULONG                      radioInterface,
   ULONG                      activeBandClass,
   ULONG                      activeChannel );

// LU reject callback function
typedef void (* tFNLUReject)(
   ULONG                      serviceDomain,
   ULONG                      rejectCause );

// New SMS message callback function
typedef void (* tFNNewSMS)(
   ULONG                      storageType,
   ULONG                      messageIndex );

// New NMEA sentence callback function
typedef void (* tFNNewNMEA)( LPCSTR pNMEA );

// New NMEA sentence plus mode callback function
typedef void (* tFNNewNMEAPlus)(
   LPCSTR                     pNMEA,
   ULONG                      mode );

// PDS session state callback function
typedef void (* tFNPDSState)(
   ULONG                      enabledStatus,
   ULONG                      trackingStatus );

// CAT event callback function
typedef void (* tFNCATEvent)(
   ULONG                      eventID,
   ULONG                      eventLen,
   BYTE *                     pEventData );

// OMA-DM network initiated alert callback function
typedef void (* tFNOMADMAlert)(
   ULONG                      sessionType,
   USHORT                     sessionID );

// OMA-DM state callback function
typedef void (* tFNOMADMState)(
   ULONG                      sessionState,
   ULONG                      failureReason );


namespace gobi {

enum PowerMode {
  kOnline = 0,
  kLowPower = 1,
  kFactoryTest = 2,
  kOffline = 3,
  kReset = 4,
  kPowerOff = 5,
  kPersistentLowPower = 6, // airplane mode
};

enum RegistrationState {
  kUnregistered = 0,
  kRegistered = 1,
  kSearching = 2,
  kRegistrationDenied = 3,
  kRegistrationStateUnknown = 4
};

enum OmadmSessionType {
  kConfigure = 0,
  kPrlUpdate = 1,
  kHandsFreeActivation = 2,
};

enum OmadmSessionState {
  // Table 2-11 of QC WWAN CM API
  kOmadmComplete = 0,
  kOmadmUpdateInformationUnavailable = 1,
  kOmadmFailed = 2,

  // Artificial state: The last state that signifies that the OMADM
  // session is over.
  kOmadmMaxFinal = 2,
  kOmadmRetrying = 3,
  kOmadmConnecting = 4,
  kOmadmConnected = 5,
  kOmadmAuthenticated = 6,
  kOmadmMdnDownloaded = 7,
  kOmadmMsidDownloaded = 8,
  kOmadmPrlDownloaded = 9,
  kOmadmMipProfiledDownloaded = 10,
};

enum ActivationState {
  kNotActivated = 0,
  kActivated = 1,
  kActivationConnecting = 2,
  kActivationConnected = 3,
  kOtaspAuthenticated = 4,
  kOtaspNamDownloaded = 5,
  kOtaspMdnDownloaded = 6,
  kOtaspImsiDownloaded = 7,
  kOtaspPrlDownloaded = 8,
  kOtaspSpcDownloaded = 9,
  kOtaspSettingsCommitted = 10,
};

enum GsmRegistrationType {
  kRegistrationTypeAutomatic = 1,
  kRegistrationTypeManual = 2
};

enum SessionState {
  kDisconnected = 1,
  kConnected = 2,
  kSuspended = 3,
  kAuthenticating = 4
};

enum RadioInterfaceTechnology {
  kRfiNoService = 0,
  kRfiCdma1xRtt = 1,
  kRfiCdmaEvdo = 2,
  kRfiAmps = 3,
  kRfiGsm = 4,
  kRfiUmts = 5
};

enum DataBearerTechnology {
  kDataBearerCdma1xRtt = 1,
  kDataBearerCdmaEvdo = 2,
  kDataBearerGprs = 3,
  kDataBearerWcdma = 4,
  kDataBearerCdmaEvdoRevA = 5,
  kDataBearerEdge = 6,
  kDataBearerHsdpaDlWcdmaUl = 7,
  kDataBearerWcdmaDlUsupaUl = 8,
  kDataBearerHsdpaDlHsupaUl = 9
};

enum DataCapability {
  kDataCapGprs = 1,
  kDataCapEdge = 2,
  kDataCapHsdpa = 3,
  kDataCapHsupa = 4,
  kDataCapWcdma = 5,
  kDataCapCdma1xRtt = 6,
  kDataCapCdmaEvdoRev0 = 7,
  kDataCapCdmaEvdoRevA = 8,
  kDataCapGsm = 9
};

enum ConfiguredTechnology {
  kConfigurationCdma = 0,
  kConfigurationUmts = 1,
  kConfigurationUnknownTechnology = 0xffffffff
};

enum RoamingState {
  kRoaming = 0,
  kHome = 1,
  kRoamingPartner = 2
};

// Selected return codes from table 2-3 of QC WWAN CM API
enum ReturnCode {
  kGeneralError = 1,
  kErrorSendingQmiRequest = 12,
  kErrorReceivingQmiRequest = 13,
  kNoAvailableSignal = 29,
  kErrorNeedsReset = 34,
  kCallFailed = 1014,
  kNotProvisioned = 1016,
  kNotSupportedByNetwork = 1024,
  kNotSupportedByDevice = 1025,
  kOperationHasNoEffect = 1026,
  kNoTrackingSessionHasBeenStarted = 1065,
  kInformationElementUnavailable = 1074,
  kQMIHardwareRestricted = 1083
};

enum DormancyStatus {
  kDormant = 1,
  kNotDormant = 2
};

// Selected call failure reasons from table 2-4 "Call end reason codes"
enum CallEndReason {
  kClientEndedCall = 2,
};

typedef struct {
  ULONG radioInterface;
  ULONG activeBandClass;
  ULONG activeChannel;
} RfInfoInstance;

typedef struct {
  WORD mcc;
  WORD mnc;
  ULONG inUse;
  ULONG roaming;
  ULONG forbidden;
  ULONG preferred;
  char description[255];
} GsmNetworkInfoInstance;

struct DeviceElement {
  char deviceNode[256];
  char deviceKey[16];
};

struct ReentrancyGroup;

class Sdk {
 public:
  typedef void (*SdkErrorSink)(const std::string &modem_path,
                               const std::string &sdk_function,
                               ULONG error);

  // SdkErrorSink is called if an SDK function returns an error.
  // The GobiModem class uses this to decide if an error warrants
  // resetting the modem
  Sdk(SdkErrorSink sink) : sdk_error_sink_(sink) {}
  virtual ~Sdk() {}

  virtual void set_current_modem_path(const std::string &path) {
    current_modem_path_ = path;
  }

  virtual void Init();

  virtual ULONG QCWWANEnumerateDevices(
      BYTE *                     pDevicesSize,
      BYTE *                     pDevices);

  virtual ULONG QCWWANConnect(
      CHAR *                     pDeviceNode,
      CHAR *                     pDeviceKey);

  virtual ULONG QCWWANDisconnect();

  virtual ULONG QCWWANGetConnectedDeviceID(
      ULONG                      deviceNodeSize,
      CHAR *                     pDeviceNode,
      ULONG                      deviceKeySize,
      CHAR *                     pDeviceKey);

  virtual ULONG GetSessionState(ULONG * pState);

  virtual ULONG GetSessionDuration(ULONGLONG * pDuration);

  virtual ULONG GetDormancyState(ULONG * pState);

  virtual ULONG GetAutoconnect(ULONG * pSetting);

  virtual ULONG SetAutoconnect(ULONG setting);

  virtual ULONG SetDefaultProfile(
      ULONG                      profileType,
      ULONG *                    pPDPType,
      ULONG *                    pIPAddress,
      ULONG *                    pPrimaryDNS,
      ULONG *                    pSecondaryDNS,
      ULONG *                    pAuthentication,
      CHAR *                     pName,
      CHAR *                     pAPNName,
      CHAR *                     pUsername,
      CHAR *                     pPassword);

  virtual ULONG GetDefaultProfile(
      ULONG                      profileType,
      ULONG *                    pPDPType,
      ULONG *                    pIPAddress,
      ULONG *                    pPrimaryDNS,
      ULONG *                    pSecondaryDNS,
      ULONG *                    pAuthentication,
      BYTE                       nameSize,
      CHAR *                     pName,
      BYTE                       apnSize,
      CHAR *                     pAPNName,
      BYTE                       userSize,
      CHAR *                     pUsername);

  virtual ULONG StartDataSession(
      ULONG *                    pTechnology,
      const CHAR *               pAPNName,
      ULONG *                    pAuthentication,
      const CHAR *               pUsername,
      const CHAR *               pPassword,
      ULONG *                    pSessionId,
      ULONG *                    pFailureReason);

  virtual ULONG StopDataSession(ULONG sessionId);

  virtual ULONG GetIPAddress(ULONG * pIPAddress);

  virtual ULONG GetConnectionRate(
      ULONG *                    pCurrentChannelTXRate,
      ULONG *                    pCurrentChannelRXRate,
      ULONG *                    pMaxChannelTXRate,
      ULONG *                    pMaxChannelRXRate);

  virtual ULONG GetPacketStatus(
      ULONG *                    pTXPacketSuccesses,
      ULONG *                    pRXPacketSuccesses,
      ULONG *                    pTXPacketErrors,
      ULONG *                    pRXPacketErrors,
      ULONG *                    pTXPacketOverflows,
      ULONG *                    pRXPacketOverflows);

  virtual ULONG GetByteTotals(
      ULONGLONG *                pTXTotalBytes,
      ULONGLONG *                pRXTotalBytes);

  virtual ULONG SetMobileIP(ULONG mode);

  virtual ULONG GetMobileIP(ULONG * pMode);

  virtual ULONG SetActiveMobileIPProfile(
      CHAR *                     pSPC,
      BYTE                       index);

  virtual ULONG GetActiveMobileIPProfile(BYTE * pIndex);

  virtual ULONG SetMobileIPProfile(
      CHAR *                     pSPC,
      BYTE                       index,
      BYTE *                     pEnabled,
      ULONG *                    pAddress,
      ULONG *                    pPrimaryHA,
      ULONG *                    pSecondaryHA,
      BYTE *                     pRevTunneling,
      CHAR *                     pNAI,
      ULONG *                    pHASPI,
      ULONG *                    pAAASPI,
      CHAR *                     pMNHA,
      CHAR *                     pMNAAA);

  virtual ULONG GetMobileIPProfile(
      BYTE                       index,
      BYTE *                     pEnabled,
      ULONG *                    pAddress,
      ULONG *                    pPrimaryHA,
      ULONG *                    pSecondaryHA,
      BYTE *                     pRevTunneling,
      BYTE                       naiSize,
      CHAR *                     pNAI,
      ULONG *                    pHASPI,
      ULONG *                    pAAASPI,
      ULONG *                    pHAState,
      ULONG *                    pAAAState);

  virtual ULONG SetMobileIPParameters(
      CHAR *                     pSPC,
      ULONG *                    pMode,
      BYTE *                     pRetryLimit,
      BYTE *                     pRetryInterval,
      BYTE *                     pReRegPeriod,
      BYTE *                     pReRegTraffic,
      BYTE *                     pHAAuthenticator,
      BYTE *                     pHA2002bis);

  virtual ULONG GetMobileIPParameters(
      ULONG *                    pMode,
      BYTE *                     pRetryLimit,
      BYTE *                     pRetryInterval,
      BYTE *                     pReRegPeriod,
      BYTE *                     pReRegTraffic,
      BYTE *                     pHAAuthenticator,
      BYTE *                     pHA2002bis);

  virtual ULONG GetLastMobileIPError(ULONG * pError);

  virtual ULONG GetANAAAAuthenticationStatus(ULONG * pStatus);

  virtual ULONG GetSignalStrengths(
      ULONG *                    pArraySizes,
      INT8 *                     pSignalStrengths,
      ULONG *                    pRadioInterfaces);

  virtual ULONG GetRFInfo(
      BYTE *                     pInstanceSize,
      BYTE *                     pInstances);

  virtual ULONG PerformNetworkScan(
      BYTE *                     pInstanceSize,
      BYTE *                     pInstances);

  virtual ULONG InitiateNetworkRegistration(
      ULONG                      regType,
      WORD                       mcc,
      WORD                       mnc,
      ULONG                      rat);

  virtual ULONG InitiateDomainAttach(ULONG action);

  virtual ULONG GetServingNetwork(
      ULONG *                    pRegistrationState,
      ULONG *                    pRAN,
      BYTE *                     pRadioIfacesSize,
      BYTE *                     pRadioIfaces,
      ULONG *                    pRoaming,
      WORD *                     pMCC,
      WORD *                     pMNC,
      BYTE                       nameSize,
      CHAR *                     pName);

  virtual ULONG GetServingNetworkCapabilities(
      BYTE *                     pDataCapsSize,
      BYTE *                     pDataCaps);

  virtual ULONG GetDataBearerTechnology(ULONG * pDataBearer);

  virtual ULONG GetHomeNetwork(
      WORD *                     pMCC,
      WORD *                     pMNC,
      BYTE                       nameSize,
      CHAR *                     pName,
      WORD *                     pSID,
      WORD *                     pNID);

  virtual ULONG SetNetworkPreference(
      ULONG                      technologyPref,
      ULONG                      duration);

  virtual ULONG GetNetworkPreference(
      ULONG *                    pTechnologyPref,
      ULONG *                    pDuration,
      ULONG *                    pPersistentTechnologyPref);

  virtual ULONG SetCDMANetworkParameters(
      CHAR *                     pSPC,
      BYTE *                     pForceRev0,
      BYTE *                     pCustomSCP,
      ULONG *                    pProtocol,
      ULONG *                    pBroadcast,
      ULONG *                    pApplication,
      ULONG *                    pRoaming);

  virtual ULONG GetCDMANetworkParameters(
      BYTE *                     pSCI,
      BYTE *                     pSCM,
      BYTE *                     pRegHomeSID,
      BYTE *                     pRegForeignSID,
      BYTE *                     pRegForeignNID,
      BYTE *                     pForceRev0,
      BYTE *                     pCustomSCP,
      ULONG *                    pProtocol,
      ULONG *                    pBroadcast,
      ULONG *                    pApplication,
      ULONG *                    pRoaming);

  virtual ULONG GetACCOLC(BYTE * pACCOLC);

  virtual ULONG SetACCOLC(
      CHAR *                     pSPC,
      BYTE                       accolc);

  virtual ULONG GetDeviceCapabilities(
      ULONG *                    pMaxTXChannelRate,
      ULONG *                    pMaxRXChannelRate,
      ULONG *                    pDataServiceCapability,
      ULONG *                    pSimCapability,
      ULONG *                    pRadioIfacesSize,
      BYTE *                     pRadioIfaces);

  virtual ULONG GetManufacturer(
      BYTE                       stringSize,
      CHAR *                     pString);

  virtual ULONG GetModelID(
      BYTE                       stringSize,
      CHAR *                     pString);

  virtual ULONG GetFirmwareRevision(
      BYTE                       stringSize,
      CHAR *                     pString);

  virtual ULONG GetFirmwareRevisions(
      BYTE                       amssSize,
      CHAR *                     pAMSSString,
      BYTE                       bootSize,
      CHAR *                     pBootString,
      BYTE                       priSize,
      CHAR *                     pPRIString);

  virtual ULONG GetFirmwareInfo(
      ULONG *                    pFirmwareID,
      ULONG *                    pTechnology,
      ULONG *                    pCarrier,
      ULONG *                    pRegion,
      ULONG *                    pGPSCapability);

  virtual ULONG GetVoiceNumber(
      BYTE                       voiceNumberSize,
      CHAR *                     pVoiceNumber,
      BYTE                       minSize,
      CHAR *                     pMIN);

  virtual ULONG GetIMSI(
      BYTE                       stringSize,
      CHAR *                     pString);

  virtual ULONG GetSerialNumbers(
      BYTE                       esnSize,
      CHAR *                     pESNString,
      BYTE                       imeiSize,
      CHAR *                     pIMEIString,
      BYTE                       meidSize,
      CHAR *                     pMEIDString);

  virtual ULONG SetLock(
      ULONG                      state,
      CHAR *                     pCurrentPIN);

  virtual ULONG QueryLock(ULONG * pState);

  virtual ULONG ChangeLockPIN(
      CHAR *                     pCurrentPIN,
      CHAR *                     pDesiredPIN);

  virtual ULONG GetHardwareRevision(
      BYTE                       stringSize,
      CHAR *                     pString);

  virtual ULONG GetPRLVersion(WORD * pPRLVersion);

  virtual ULONG GetERIFile(
      ULONG *                    pFileSize,
      BYTE *                     pFile);

  virtual ULONG ActivateAutomatic(const CHAR * pActivationCode);

  virtual ULONG ActivateManual(
      const CHAR *               pSPC,
      WORD                       sid,
      const CHAR *               pMDN,
      const CHAR *               pMIN,
      ULONG                      prlSize,
      BYTE *                     pPRL,
      const CHAR *               pMNHA,
      const CHAR *               pMNAAA);

  virtual ULONG ResetToFactoryDefaults(CHAR * pSPC);

  virtual ULONG GetActivationState(ULONG * pActivationState);

  virtual ULONG SetPower(ULONG powerMode);

  virtual ULONG GetPower(ULONG * pPowerMode);

  virtual ULONG GetOfflineReason(
      ULONG *                    pReasonMask,
      ULONG *                    pbPlatform);

  virtual ULONG GetNetworkTime(
      ULONGLONG *                pTimeCount,
      ULONG *                    pTimeSource);

  virtual ULONG ValidateSPC(CHAR * pSPC);

  virtual ULONG DeleteSMS(
      ULONG                      storageType,
      ULONG *                    pMessageIndex,
      ULONG *                    pMessageTag);

  virtual ULONG GetSMSList(
      ULONG                      storageType,
      ULONG *                    pRequestedTag,
      ULONG *                    pMessageListSize,
      BYTE *                     pMessageList);

  virtual ULONG GetSMS(
      ULONG                      storageType,
      ULONG                      messageIndex,
      ULONG *                    pMessageTag,
      ULONG *                    pMessageFormat,
      ULONG *                    pMessageSize,
      BYTE *                     pMessage);

  virtual ULONG ModifySMSStatus(
      ULONG                      storageType,
      ULONG                      messageIndex,
      ULONG                      messageTag);

  virtual ULONG SaveSMS(
      ULONG                      storageType,
      ULONG                      messageFormat,
      ULONG                      messageSize,
      BYTE *                     pMessage,
      ULONG *                    pMessageIndex);

  virtual ULONG SendSMS(
      ULONG                      messageFormat,
      ULONG                      messageSize,
      BYTE *                     pMessage,
      ULONG *                    pMessageFailureCode);

  virtual ULONG GetSMSCAddress(
      BYTE                       addressSize,
      CHAR *                     pSMSCAddress,
      BYTE                       typeSize,
      CHAR *                     pSMSCType);

  virtual ULONG SetSMSCAddress(
      CHAR *                     pSMSCAddress,
      CHAR *                     pSMSCType);

  virtual ULONG UIMSetPINProtection(
      ULONG                      id,
      ULONG                      bEnable,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG UIMVerifyPIN(
      ULONG                      id,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG UIMUnblockPIN(
      ULONG                      id,
      CHAR *                     pPUKValue,
      CHAR *                     pNewValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG UIMChangePIN(
      ULONG                      id,
      CHAR *                     pOldValue,
      CHAR *                     pNewValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG UIMGetPINStatus(
      ULONG                      id,
      ULONG *                    pStatus,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG UIMGetICCID(
      BYTE                       stringSize,
      CHAR *                     pString);
  virtual ULONG UIMGetControlKeyStatus(
      ULONG                      id,
      ULONG *                    pStatus,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG UIMSetControlKeyProtection(
      ULONG                      id,
      ULONG                      status,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft);

  virtual ULONG UIMUnblockControlKey(
      ULONG                      id,
      CHAR *                     pValue,
      ULONG *                    pUnblockRetriesLeft);

  virtual ULONG GetPDSState(
      ULONG *                    pEnabled,
      ULONG *                    pTracking);

  virtual ULONG SetPDSState(ULONG enable);

  virtual ULONG PDSInjectTimeReference(
      ULONGLONG                  systemTime,
      USHORT                     systemDiscontinuities);

  virtual ULONG GetPDSDefaults(
      ULONG *                    pOperation,
      BYTE *                     pTimeout,
      ULONG *                    pInterval,
      ULONG *                    pAccuracy);

  virtual ULONG SetPDSDefaults(
      ULONG                      operation,
      BYTE                       timeout,
      ULONG                      interval,
      ULONG                      accuracy);

  virtual ULONG GetXTRAAutomaticDownload(
      ULONG *                    pbEnabled,
      USHORT *                   pInterval);

  virtual ULONG SetXTRAAutomaticDownload(
      ULONG                      bEnabled,
      USHORT                     interval);

  virtual ULONG GetXTRANetwork(ULONG * pPreference);

  virtual ULONG SetXTRANetwork(ULONG preference);

  virtual ULONG GetXTRAValidity(
      USHORT *                   pGPSWeek,
      USHORT *                   pGPSWeekOffset,
      USHORT *                   pDuration);

  virtual ULONG ForceXTRADownload();

  virtual ULONG GetAGPSConfig(
      ULONG *                    pServerAddress,
      ULONG *                    pServerPort);

  virtual ULONG SetAGPSConfig(
      ULONG                      serverAddress,
      ULONG                      serverPort);

  virtual ULONG GetServiceAutomaticTracking(ULONG * pbAuto);

  virtual ULONG SetServiceAutomaticTracking(ULONG bAuto);

  virtual ULONG GetPortAutomaticTracking(ULONG * pbAuto);

  virtual ULONG SetPortAutomaticTracking(ULONG bAuto);

  virtual ULONG ResetPDSData(
      ULONG *                    pGPSDataMask,
      ULONG *                    pCellDataMask);

  virtual ULONG CATSendTerminalResponse(
      ULONG                      refID,
      ULONG                      dataLen,
      BYTE *                     pData);

  virtual ULONG CATSendEnvelopeCommand(
      ULONG                      cmdID,
      ULONG                      dataLen,
      BYTE *                     pData);

  virtual ULONG GetSMSWake(
      ULONG *                    pbEnabled,
      ULONG *                    pWakeMask);

  virtual ULONG SetSMSWake(
      ULONG                      bEnable,
      ULONG                      wakeMask);

  virtual ULONG OMADMStartSession(ULONG sessionType);

  virtual ULONG OMADMCancelSession();

  virtual ULONG OMADMGetSessionInfo(
      ULONG *                    pSessionState,
      ULONG *                    pSessionType,
      ULONG *                    pFailureReason,
      BYTE *                     pRetryCount,
      WORD *                     pSessionPause,
      WORD *                     pTimeRemaining);

  virtual ULONG OMADMGetPendingNIA(
      ULONG *                    pSessionType,
      USHORT *                   pSessionID);

  virtual ULONG OMADMSendSelection(
      ULONG                      selection,
      USHORT                     sessionID);

  virtual ULONG OMADMGetFeatureSettings(
      ULONG *                    pbProvisioning,
      ULONG *                    pbPRLUpdate);

  virtual ULONG OMADMSetProvisioningFeature(
      ULONG                      bProvisioning);

  virtual ULONG OMADMSetPRLUpdateFeature(
      ULONG                      bPRLUpdate);

  virtual ULONG UpgradeFirmware(CHAR * pDestinationPath);

  virtual ULONG GetImageInfo(
      CHAR *                     pPath,
      ULONG *                    pFirmwareID,
      ULONG *                    pTechnology,
      ULONG *                    pCarrier,
      ULONG *                    pRegion,
      ULONG *                    pGPSCapability);

  virtual ULONG GetImageStore(
      WORD                       pathSize,
      CHAR *                     pImageStorePath);

  virtual ULONG SetSessionStateCallback(tFNSessionState pCallback);

  virtual ULONG SetByteTotalsCallback(
      tFNByteTotals              pCallback,
      BYTE                       interval);

  virtual ULONG SetDataCapabilitiesCallback(
      tFNDataCapabilities        pCallback);

  virtual ULONG SetDataBearerCallback(tFNDataBearer pCallback);

  virtual ULONG SetDormancyStatusCallback(
      tFNDormancyStatus          pCallback);

  virtual ULONG SetMobileIPStatusCallback(
      tFNMobileIPStatus          pCallback);

  virtual ULONG SetActivationStatusCallback(
      tFNActivationStatus        pCallback);

  virtual ULONG SetPowerCallback(tFNPower pCallback);

  virtual ULONG SetRoamingIndicatorCallback(
      tFNRoamingIndicator        pCallback);

  virtual ULONG SetSignalStrengthCallback(
      tFNSignalStrength          pCallback,
      BYTE                       thresholdsSize,
      INT8 *                     pThresholds);

  virtual ULONG SetRFInfoCallback(tFNRFInfo pCallback);

  virtual ULONG SetLURejectCallback(tFNLUReject pCallback);

  virtual ULONG SetNewSMSCallback(tFNNewSMS pCallback);

  virtual ULONG SetNMEACallback(tFNNewNMEA pCallback);

  virtual ULONG SetNMEAPlusCallback(tFNNewNMEAPlus pCallback);

  virtual ULONG SetPDSStateCallback(tFNPDSState pCallback);

  virtual ULONG SetCATEventCallback(
      tFNCATEvent                pCallback,
      ULONG                      eventMask,
      ULONG *                    pErrorMask);

  virtual ULONG SetOMADMAlertCallback(tFNOMADMAlert pCallback);

  virtual ULONG SetOMADMStateCallback(tFNOMADMState pCallback);

 protected:

  // Make a temporary copy of a char *, preserving the NULL-ness of the
  // input.
  struct TemporaryCopier {
   public:
    explicit TemporaryCopier(const char *in) {
      if (!in) {
        str_.reset(NULL);
      } else {
        str_.reset(strdup(in));
      }
    }
    ~TemporaryCopier() {
      // We sometimes pass sensitive information
      if (str_.get()) {
        memset(str_.get(), '\0', strlen(str_.get()));
      }
    }
    char *get() {
      return str_.get();
    }

   private:
    scoped_ptr_malloc<char> str_;
    DISALLOW_COPY_AND_ASSIGN(TemporaryCopier);
  };

  // The CMAPI is split into groups.  Functions in multiple groups can
  // be called simultaneously, but each group is nonreentrant against
  // itself.  This machinery verifies that we don't violate these
  // restrictions.

  void EnterSdk(const char *function_name);
  void LeaveSdk(const char *function_name);

  void InitGetServiceFromName(const char *service_map[]);
  int GetServiceFromName(const char *name);

  // Given a starting service of begin, what services should we cover?
  // Returns max service if supplied with kServiceBase, else begin + 1
  int GetServiceBound(int begin);

  std::map<int, const char*> index_to_service_name_;
  std::map<std::string, int> name_to_service_;
  int service_index_upper_bound_;

  // mutex-protected mapping from a service index to the
  // currently-executing function in that service.
  pthread_mutex_t service_to_function_mutex_;
  std::vector<const char*> service_to_function_;

  SdkErrorSink sdk_error_sink_;
  std::string current_modem_path_;

  friend class CallWrapper;
  FRIEND_TEST(GobiSdkTest, BaseClosesAllDeathTest);
  FRIEND_TEST(GobiSdkTest, EnterLeaveDeathTest);
  FRIEND_TEST(GobiSdkTest, InitGetServiceFromNameDeathTest);
  FRIEND_TEST(GobiSdkTest, InitGetServiceFromName);
  FRIEND_TEST(GobiSdkTest, TemporaryCopier);

 private:
  DISALLOW_COPY_AND_ASSIGN(Sdk);
};  // Class Sdk
}   // Namespace Gobi

#endif /* PLUGIN_GOBI_SDK_WRAPPER_H_ */

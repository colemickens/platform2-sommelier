#ifndef PLUGIN_GOBI_SDK_WRAPPER_H_
#define PLUGIN_GOBI_SDK_WRAPPER_H_

/*===========================================================================
  FILE:
  gobi_sdk_wrapper.h,
  derived from
  QCWWANCMAPI2k.h

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

#include "QCWWANCMAPI2k.h"

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

// Selected return codes from table 2-3 of QC WWAN CM API
enum ReturnCode {
  kNotProvisioned = 1016,
  kNotSupportedByNetwork = 1024,
  kNotSupportedByDevice = 1025,
};

class Sdk {
 public:
  virtual ~Sdk() {}

  virtual ULONG QCWWANEnumerateDevices(
      BYTE *                     pDevicesSize,
      BYTE *                     pDevices) {
    return ::QCWWANEnumerateDevices(pDevicesSize,
                                    pDevices);
  }

  virtual ULONG QCWWANConnect(
      CHAR *                     pDeviceNode,
      CHAR *                     pDeviceKey) {
    return ::QCWWANConnect(pDeviceNode,
                           pDeviceKey);
  }

  virtual ULONG QCWWANDisconnect() {
    return ::QCWWANDisconnect();
  }

  virtual ULONG QCWWANGetConnectedDeviceID(
      ULONG                      deviceNodeSize,
      CHAR *                     pDeviceNode,
      ULONG                      deviceKeySize,
      CHAR *                     pDeviceKey) {
    return ::QCWWANGetConnectedDeviceID(
        deviceNodeSize,
        pDeviceNode,
        deviceKeySize,
        pDeviceKey);
  }

  virtual ULONG GetSessionState(ULONG * pState) {
    return ::GetSessionState(pState);
  }

  virtual ULONG GetSessionDuration(ULONGLONG * pDuration) {
    return ::GetSessionDuration(pDuration);
  }

  virtual ULONG GetDormancyState(ULONG * pState) {
    return ::GetDormancyState(pState);
  }

  virtual ULONG GetAutoconnect(ULONG * pSetting) {
    return ::GetAutoconnect(pSetting);
  }

  virtual ULONG SetAutoconnect(ULONG setting) {
    return ::SetAutoconnect(setting);
  }

  ULONG SetDefaultProfile(
      ULONG                      profileType,
      ULONG *                    pPDPType,
      ULONG *                    pIPAddress,
      ULONG *                    pPrimaryDNS,
      ULONG *                    pSecondaryDNS,
      ULONG *                    pAuthentication,
      CHAR *                     pName,
      CHAR *                     pAPNName,
      CHAR *                     pUsername,
      CHAR *                     pPassword) {
    return ::SetDefaultProfile(
        profileType,
        pPDPType,
        pIPAddress,
        pPrimaryDNS,
        pSecondaryDNS,
        pAuthentication,
        pName,
        pAPNName,
        pUsername,
        pPassword);
  }

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
      CHAR *                     pUsername) {
    return ::GetDefaultProfile(
        profileType,
        pPDPType,
        pIPAddress,
        pPrimaryDNS,
        pSecondaryDNS,
        pAuthentication,
        nameSize,
        pName,
        apnSize,
        pAPNName,
        userSize,
        pUsername);
  }

  virtual ULONG StartDataSession(
      ULONG *                    pTechnology,
      ULONG *                    pPrimaryDNS,
      ULONG *                    pSecondaryDNS,
      ULONG *                    pPrimaryNBNS,
      ULONG *                    pSecondaryNBNS,
      CHAR *                     pAPNName,
      ULONG *                    pIPAddress,
      ULONG *                    pAuthentication,
      CHAR *                     pUsername,
      CHAR *                     pPassword,
      ULONG *                    pSessionId,
      ULONG *                    pFailureReason) {
    return ::StartDataSession(
        pTechnology,
        pPrimaryDNS,
        pSecondaryDNS,
        pPrimaryNBNS,
        pSecondaryNBNS,
        pAPNName,
        pIPAddress,
        pAuthentication,
        pUsername,
        pPassword,
        pSessionId,
        pFailureReason);
  }

  virtual ULONG StopDataSession(ULONG sessionId) {
    return ::StopDataSession(sessionId);
  }

  virtual ULONG GetIPAddress(ULONG * pIPAddress) {
    return ::GetIPAddress(pIPAddress);
  }

  virtual ULONG GetConnectionRate(
      ULONG *                    pCurrentChannelTXRate,
      ULONG *                    pCurrentChannelRXRate,
      ULONG *                    pMaxChannelTXRate,
      ULONG *                    pMaxChannelRXRate) {
    return ::GetConnectionRate(
        pCurrentChannelTXRate,
        pCurrentChannelRXRate,
        pMaxChannelTXRate,
        pMaxChannelRXRate);
  }

  virtual ULONG GetPacketStatus(
      ULONG *                    pTXPacketSuccesses,
      ULONG *                    pRXPacketSuccesses,
      ULONG *                    pTXPacketErrors,
      ULONG *                    pRXPacketErrors,
      ULONG *                    pTXPacketOverflows,
      ULONG *                    pRXPacketOverflows) {
    return ::GetPacketStatus(
        pTXPacketSuccesses,
        pRXPacketSuccesses,
        pTXPacketErrors,
        pRXPacketErrors,
        pTXPacketOverflows,
        pRXPacketOverflows);
  }

  virtual ULONG GetByteTotals(
      ULONGLONG *                pTXTotalBytes,
      ULONGLONG *                pRXTotalBytes) {
    return ::GetByteTotals(
        pTXTotalBytes,
        pRXTotalBytes);
  }

  virtual ULONG SetMobileIP(ULONG mode) {
    return ::SetMobileIP(mode);
  }

  virtual ULONG GetMobileIP(ULONG * pMode) {
    return ::GetMobileIP(pMode);
  }

  virtual ULONG SetActiveMobileIPProfile(
      CHAR *                     pSPC,
      BYTE                       index) {
    return ::SetActiveMobileIPProfile(
        pSPC,
        index);
  }

  virtual ULONG GetActiveMobileIPProfile(BYTE * pIndex) {
    return ::GetActiveMobileIPProfile(pIndex);
  }

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
      CHAR *                     pMNAAA) {
    return ::SetMobileIPProfile(
        pSPC,
        index,
        pEnabled,
        pAddress,
        pPrimaryHA,
        pSecondaryHA,
        pRevTunneling,
        pNAI,
        pHASPI,
        pAAASPI,
        pMNHA,
        pMNAAA);
  }

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
      ULONG *                    pAAAState) {
    return ::GetMobileIPProfile(
        index,
        pEnabled,
        pAddress,
        pPrimaryHA,
        pSecondaryHA,
        pRevTunneling,
        naiSize,
        pNAI,
        pHASPI,
        pAAASPI,
        pHAState,
        pAAAState);
  }

  virtual ULONG SetMobileIPParameters(
      CHAR *                     pSPC,
      ULONG *                    pMode,
      BYTE *                     pRetryLimit,
      BYTE *                     pRetryInterval,
      BYTE *                     pReRegPeriod,
      BYTE *                     pReRegTraffic,
      BYTE *                     pHAAuthenticator,
      BYTE *                     pHA2002bis) {
    return ::SetMobileIPParameters(
        pSPC,
        pMode,
        pRetryLimit,
        pRetryInterval,
        pReRegPeriod,
        pReRegTraffic,
        pHAAuthenticator,
        pHA2002bis);
  }

  virtual ULONG GetMobileIPParameters(
      ULONG *                    pMode,
      BYTE *                     pRetryLimit,
      BYTE *                     pRetryInterval,
      BYTE *                     pReRegPeriod,
      BYTE *                     pReRegTraffic,
      BYTE *                     pHAAuthenticator,
      BYTE *                     pHA2002bis) {
    return ::GetMobileIPParameters(
        pMode,
        pRetryLimit,
        pRetryInterval,
        pReRegPeriod,
        pReRegTraffic,
        pHAAuthenticator,
        pHA2002bis);
  }

  virtual ULONG GetLastMobileIPError(ULONG * pError) {
    return ::GetLastMobileIPError(pError);
  }

  virtual ULONG GetANAAAAuthenticationStatus(ULONG * pStatus) {
    return ::GetANAAAAuthenticationStatus(pStatus);
  }

  virtual ULONG GetSignalStrengths(
      ULONG *                    pArraySizes,
      INT8 *                     pSignalStrengths,
      ULONG *                    pRadioInterfaces) {
    return ::GetSignalStrengths(
        pArraySizes,
        pSignalStrengths,
        pRadioInterfaces);
  }

  virtual ULONG GetRFInfo(
      BYTE *                     pInstanceSize,
      BYTE *                     pInstances) {
    return ::GetRFInfo(
        pInstanceSize,
        pInstances);
  }

  virtual ULONG PerformNetworkScan(
      BYTE *                     pInstanceSize,
      BYTE *                     pInstances) {
    return ::PerformNetworkScan(
        pInstanceSize,
        pInstances);
  }

  virtual ULONG InitiateNetworkRegistration(
      ULONG                      regType,
      WORD                       mcc,
      WORD                       mnc,
      ULONG                      rat) {
    return ::InitiateNetworkRegistration(
        regType,
        mcc,
        mnc,
        rat);
  }

  virtual ULONG InitiateDomainAttach(ULONG action) {
    return ::InitiateDomainAttach(action);
  }

  virtual ULONG GetServingNetwork(
      ULONG *                    pRegistrationState,
      ULONG *                    pCSDomain,
      ULONG *                    pPSDomain,
      ULONG *                    pRAN,
      BYTE *                     pRadioIfacesSize,
      BYTE *                     pRadioIfaces,
      ULONG *                    pRoaming,
      WORD *                     pMCC,
      WORD *                     pMNC,
      BYTE                       nameSize,
      CHAR *                     pName) {
    return ::GetServingNetwork(
        pRegistrationState,
        pCSDomain,
        pPSDomain,
        pRAN,
        pRadioIfacesSize,
        pRadioIfaces,
        pRoaming,
        pMCC,
        pMNC,
        nameSize,
        pName);
  }

  virtual ULONG GetServingNetworkCapabilities(
      BYTE *                     pDataCapsSize,
      BYTE *                     pDataCaps) {
    return ::GetServingNetworkCapabilities(
        pDataCapsSize,
        pDataCaps);
  }

  virtual ULONG GetDataBearerTechnology(ULONG * pDataBearer) {
    return ::GetDataBearerTechnology(pDataBearer);
  }

  virtual ULONG GetHomeNetwork(
      WORD *                     pMCC,
      WORD *                     pMNC,
      BYTE                       nameSize,
      CHAR *                     pName,
      WORD *                     pSID,
      WORD *                     pNID) {
    return ::GetHomeNetwork(
        pMCC,
        pMNC,
        nameSize,
        pName,
        pSID,
        pNID);
  }

  virtual ULONG SetNetworkPreference(
      ULONG                      technologyPref,
      ULONG                      duration) {
    return ::SetNetworkPreference(
        technologyPref,
        duration);
  }

  virtual ULONG GetNetworkPreference(
      ULONG *                    pTechnologyPref,
      ULONG *                    pDuration,
      ULONG *                    pPersistentTechnologyPref) {
    return ::GetNetworkPreference(
        pTechnologyPref,
        pDuration,
        pPersistentTechnologyPref);
  }

  virtual ULONG SetCDMANetworkParameters(
      CHAR *                     pSPC,
      BYTE *                     pForceRev0,
      BYTE *                     pCustomSCP,
      ULONG *                    pProtocol,
      ULONG *                    pBroadcast,
      ULONG *                    pApplication,
      ULONG *                    pRoaming) {
    return ::SetCDMANetworkParameters(
        pSPC,
        pForceRev0,
        pCustomSCP,
        pProtocol,
        pBroadcast,
        pApplication,
        pRoaming);
  }

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
      ULONG *                    pRoaming) {
    return ::GetCDMANetworkParameters(
        pSCI,
        pSCM,
        pRegHomeSID,
        pRegForeignSID,
        pRegForeignNID,
        pForceRev0,
        pCustomSCP,
        pProtocol,
        pBroadcast,
        pApplication,
        pRoaming);
  }

  virtual ULONG GetACCOLC(BYTE * pACCOLC) {
    return ::GetACCOLC(pACCOLC);
  }

  virtual ULONG SetACCOLC(
      CHAR *                     pSPC,
      BYTE                       accolc) {
    return ::SetACCOLC(
        pSPC,
        accolc);
  }

  virtual ULONG GetDeviceCapabilities(
      ULONG *                    pMaxTXChannelRate,
      ULONG *                    pMaxRXChannelRate,
      ULONG *                    pDataServiceCapability,
      ULONG *                    pSimCapability,
      ULONG *                    pRadioIfacesSize,
      BYTE *                     pRadioIfaces) {
    return ::GetDeviceCapabilities(
        pMaxTXChannelRate,
        pMaxRXChannelRate,
        pDataServiceCapability,
        pSimCapability,
        pRadioIfacesSize,
        pRadioIfaces);
  }

  virtual ULONG GetManufacturer(
      BYTE                       stringSize,
      CHAR *                     pString) {
    return ::GetManufacturer(
        stringSize,
        pString);
  }

  virtual ULONG GetModelID(
      BYTE                       stringSize,
      CHAR *                     pString) {
    return ::GetModelID(
        stringSize,
        pString);
  }

  virtual ULONG GetFirmwareRevision(
      BYTE                       stringSize,
      CHAR *                     pString) {
    return ::GetFirmwareRevision(
        stringSize,
        pString);
  }

  virtual ULONG GetFirmwareRevisions(
      BYTE                       amssSize,
      CHAR *                     pAMSSString,
      BYTE                       bootSize,
      CHAR *                     pBootString,
      BYTE                       priSize,
      CHAR *                     pPRIString) {
    return ::GetFirmwareRevisions(
        amssSize,
        pAMSSString,
        bootSize,
        pBootString,
        priSize,
        pPRIString);
  }

  virtual ULONG GetFirmwareInfo(
      ULONG *                    pFirmwareID,
      ULONG *                    pTechnology,
      ULONG *                    pCarrier,
      ULONG *                    pRegion,
      ULONG *                    pGPSCapability) {
    return ::GetFirmwareInfo(
        pFirmwareID,
        pTechnology,
        pCarrier,
        pRegion,
        pGPSCapability);
  }

  virtual ULONG GetVoiceNumber(
      BYTE                       voiceNumberSize,
      CHAR *                     pVoiceNumber,
      BYTE                       minSize,
      CHAR *                     pMIN) {
    return ::GetVoiceNumber(
        voiceNumberSize,
        pVoiceNumber,
        minSize,
        pMIN);
  }

  virtual ULONG GetIMSI(
      BYTE                       stringSize,
      CHAR *                     pString) {
    return ::GetIMSI(
        stringSize,
        pString);
  }

  virtual ULONG GetSerialNumbers(
      BYTE                       esnSize,
      CHAR *                     pESNString,
      BYTE                       imeiSize,
      CHAR *                     pIMEIString,
      BYTE                       meidSize,
      CHAR *                     pMEIDString) {
    return ::GetSerialNumbers(
        esnSize,
        pESNString,
        imeiSize,
        pIMEIString,
        meidSize,
        pMEIDString);
  }

  virtual ULONG SetLock(
      ULONG                      state,
      CHAR *                     pCurrentPIN) {
    return ::SetLock(
        state,
        pCurrentPIN);
  }

  virtual ULONG QueryLock(ULONG * pState) {
    return ::QueryLock(pState);
  }

  virtual ULONG ChangeLockPIN(
      CHAR *                     pCurrentPIN,
      CHAR *                     pDesiredPIN) {
    return ::ChangeLockPIN(
        pCurrentPIN,
        pDesiredPIN);
  }

  virtual ULONG GetHardwareRevision(
      BYTE                       stringSize,
      CHAR *                     pString) {
    return ::GetHardwareRevision(
        stringSize,
        pString);
  }

  virtual ULONG GetPRLVersion(WORD * pPRLVersion) {
    return ::GetPRLVersion(pPRLVersion);
  }

  virtual ULONG GetERIFile(
      ULONG *                    pFileSize,
      BYTE *                     pFile) {
    return ::GetERIFile(
        pFileSize,
        pFile);
  }

  // NB: CHROMIUM: modified this to const after investigating QC code
  virtual ULONG ActivateAutomatic(const CHAR * pActivationCode) {
    return ::ActivateAutomatic(const_cast<CHAR *>(pActivationCode));
  }

  virtual ULONG ActivateManual(
      CHAR *                     pSPC,
      WORD                       sid,
      CHAR *                     pMDN,
      CHAR *                     pMIN,
      ULONG                      prlSize,
      BYTE *                     pPRL,
      CHAR *                     pMNHA,
      CHAR *                     pMNAAA) {
    return ::ActivateManual(
        pSPC,
        sid,
        pMDN,
        pMIN,
        prlSize,
        pPRL,
        pMNHA,
        pMNAAA);
  }

  virtual ULONG ResetToFactoryDefaults(CHAR * pSPC) {
    return ::ResetToFactoryDefaults(pSPC);
  }

  virtual ULONG GetActivationState(ULONG * pActivationState) {
    return ::GetActivationState(pActivationState);
  }

  virtual ULONG SetPower(ULONG powerMode) {
    return ::SetPower(powerMode);
  }

  virtual ULONG GetPower(ULONG * pPowerMode) {
    return ::GetPower(pPowerMode);
  }

  virtual ULONG GetOfflineReason(
      ULONG *                    pReasonMask,
      ULONG *                    pbPlatform) {
    return ::GetOfflineReason(
        pReasonMask,
        pbPlatform);
  }

  virtual ULONG GetNetworkTime(
      ULONGLONG *                pTimeCount,
      ULONG *                    pTimeSource) {
    return ::GetNetworkTime(
        pTimeCount,
        pTimeSource);
  }

  virtual ULONG ValidateSPC(CHAR * pSPC) {
    return ::ValidateSPC(pSPC);
  }

  virtual ULONG DeleteSMS(
      ULONG                      storageType,
      ULONG *                    pMessageIndex,
      ULONG *                    pMessageTag) {
    return ::DeleteSMS(
        storageType,
        pMessageIndex,
        pMessageTag);
  }

  virtual ULONG GetSMSList(
      ULONG                      storageType,
      ULONG *                    pRequestedTag,
      ULONG *                    pMessageListSize,
      BYTE *                     pMessageList) {
    return ::GetSMSList(
        storageType,
        pRequestedTag,
        pMessageListSize,
        pMessageList);
  }

  virtual ULONG GetSMS(
      ULONG                      storageType,
      ULONG                      messageIndex,
      ULONG *                    pMessageTag,
      ULONG *                    pMessageFormat,
      ULONG *                    pMessageSize,
      BYTE *                     pMessage) {
    return ::GetSMS(
        storageType,
        messageIndex,
        pMessageTag,
        pMessageFormat,
        pMessageSize,
        pMessage);
  }

  virtual ULONG ModifySMSStatus(
      ULONG                      storageType,
      ULONG                      messageIndex,
      ULONG                      messageTag) {
    return ::ModifySMSStatus(
        storageType,
        messageIndex,
        messageTag);
  }

  virtual ULONG SaveSMS(
      ULONG                      storageType,
      ULONG                      messageFormat,
      ULONG                      messageSize,
      BYTE *                     pMessage,
      ULONG *                    pMessageIndex) {
    return ::SaveSMS(
        storageType,
        messageFormat,
        messageSize,
        pMessage,
        pMessageIndex);
  }

  virtual ULONG SendSMS(
      ULONG                      messageFormat,
      ULONG                      messageSize,
      BYTE *                     pMessage,
      ULONG *                    pMessageFailureCode) {
    return ::SendSMS(
        messageFormat,
        messageSize,
        pMessage,
        pMessageFailureCode);
  }

  virtual ULONG GetSMSCAddress(
      BYTE                       addressSize,
      CHAR *                     pSMSCAddress,
      BYTE                       typeSize,
      CHAR *                     pSMSCType) {
    return ::GetSMSCAddress(
        addressSize,
        pSMSCAddress,
        typeSize,
        pSMSCType);
  }

  virtual ULONG SetSMSCAddress(
      CHAR *                     pSMSCAddress,
      CHAR *                     pSMSCType) {
    return ::SetSMSCAddress(
        pSMSCAddress,
        pSMSCType);
  }

  virtual ULONG UIMSetPINProtection(
      ULONG                      id,
      ULONG                      bEnable,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMSetPINProtection(
        id,
        bEnable,
        pValue,
        pVerifyRetriesLeft,
        pUnblockRetriesLeft);
  }

  virtual ULONG UIMVerifyPIN(
      ULONG                      id,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMVerifyPIN(
        id,
        pValue,
        pVerifyRetriesLeft,
        pUnblockRetriesLeft);
  }

  virtual ULONG UIMUnblockPIN(
      ULONG                      id,
      CHAR *                     pPUKValue,
      CHAR *                     pNewValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMUnblockPIN(
        id,
        pPUKValue,
        pNewValue,
        pVerifyRetriesLeft,
        pUnblockRetriesLeft);
  }

  virtual ULONG UIMChangePIN(
      ULONG                      id,
      CHAR *                     pOldValue,
      CHAR *                     pNewValue,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMChangePIN(
        id,
        pOldValue,
        pNewValue,
        pVerifyRetriesLeft,
        pUnblockRetriesLeft);
  }

  virtual ULONG UIMGetPINStatus(
      ULONG                      id,
      ULONG *                    pStatus,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMGetPINStatus(
        id,
        pStatus,
        pVerifyRetriesLeft,
        pUnblockRetriesLeft);
  }

  virtual ULONG UIMGetICCID(
      BYTE                       stringSize,
      CHAR *                     pString) {
    return ::UIMGetICCID(
        stringSize,
        pString);
  }
  virtual ULONG UIMGetControlKeyStatus(
      ULONG                      id,
      ULONG *                    pStatus,
      ULONG *                    pVerifyRetriesLeft,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMGetControlKeyStatus(
        id,
        pStatus,
        pVerifyRetriesLeft,
        pUnblockRetriesLeft);
  }

  virtual ULONG UIMSetControlKeyProtection(
      ULONG                      id,
      ULONG                      status,
      CHAR *                     pValue,
      ULONG *                    pVerifyRetriesLeft) {
    return ::UIMSetControlKeyProtection(
        id,
        status,
        pValue,
        pVerifyRetriesLeft);
  }

  virtual ULONG UIMUnblockControlKey(
      ULONG                      id,
      CHAR *                     pValue,
      ULONG *                    pUnblockRetriesLeft) {
    return ::UIMUnblockControlKey(
        id,
        pValue,
        pUnblockRetriesLeft);
  }

  virtual ULONG GetPDSState(
      ULONG *                    pEnabled,
      ULONG *                    pTracking) {
    return ::GetPDSState(
        pEnabled,
        pTracking);
  }

  virtual ULONG SetPDSState(ULONG enable) {
    return ::SetPDSState(enable);
  }

  virtual ULONG PDSInjectTimeReference(
      ULONGLONG                  systemTime,
      USHORT                     systemDiscontinuities) {
    return ::PDSInjectTimeReference(
        systemTime,
        systemDiscontinuities);
  }

  virtual ULONG GetPDSDefaults(
      ULONG *                    pOperation,
      BYTE *                     pTimeout,
      ULONG *                    pInterval,
      ULONG *                    pAccuracy) {
    return ::GetPDSDefaults(
        pOperation,
        pTimeout,
        pInterval,
        pAccuracy);
  }

  virtual ULONG SetPDSDefaults(
      ULONG                      operation,
      BYTE                       timeout,
      ULONG                      interval,
      ULONG                      accuracy) {
    return ::SetPDSDefaults(
        operation,
        timeout,
        interval,
        accuracy);
  }

  virtual ULONG GetXTRAAutomaticDownload(
      ULONG *                    pbEnabled,
      USHORT *                   pInterval) {
    return ::GetXTRAAutomaticDownload(
        pbEnabled,
        pInterval);
  }

  virtual ULONG SetXTRAAutomaticDownload(
      ULONG                      bEnabled,
      USHORT                     interval) {
    return ::SetXTRAAutomaticDownload(
        bEnabled,
        interval);
  }

  virtual ULONG GetXTRANetwork(ULONG * pPreference) {
    return ::GetXTRANetwork(pPreference);
  }

  virtual ULONG SetXTRANetwork(ULONG preference) {
    return ::SetXTRANetwork(preference);
  }

  virtual ULONG GetXTRAValidity(
      USHORT *                   pGPSWeek,
      USHORT *                   pGPSWeekOffset,
      USHORT *                   pDuration) {
    return ::GetXTRAValidity(
        pGPSWeek,
        pGPSWeekOffset,
        pDuration);
  }

  virtual ULONG ForceXTRADownload() {
    return ::ForceXTRADownload();
  }

  virtual ULONG GetAGPSConfig(
      ULONG *                    pServerAddress,
      ULONG *                    pServerPort) {
    return ::GetAGPSConfig(
        pServerAddress,
        pServerPort);
  }

  virtual ULONG SetAGPSConfig(
      ULONG                      serverAddress,
      ULONG                      serverPort) {
    return ::SetAGPSConfig(
        serverAddress,
        serverPort);
  }

  virtual ULONG GetServiceAutomaticTracking(ULONG * pbAuto) {
    return ::GetServiceAutomaticTracking(pbAuto);
  }

  virtual ULONG SetServiceAutomaticTracking(ULONG bAuto) {
    return ::SetServiceAutomaticTracking(bAuto);
  }

  virtual ULONG GetPortAutomaticTracking(ULONG * pbAuto) {
    return ::GetPortAutomaticTracking(pbAuto);
  }

  virtual ULONG SetPortAutomaticTracking(ULONG bAuto) {
    return ::SetPortAutomaticTracking(bAuto);
  }

  virtual ULONG ResetPDSData(
      ULONG *                    pGPSDataMask,
      ULONG *                    pCellDataMask) {
    return ::ResetPDSData(
        pGPSDataMask,
        pCellDataMask);
  }

  virtual ULONG CATSendTerminalResponse(
      ULONG                      refID,
      ULONG                      dataLen,
      BYTE *                     pData) {
    return ::CATSendTerminalResponse(
        refID,
        dataLen,
        pData);
  }

  virtual ULONG CATSendEnvelopeCommand(
      ULONG                      cmdID,
      ULONG                      dataLen,
      BYTE *                     pData) {
    return ::CATSendEnvelopeCommand(
        cmdID,
        dataLen,
        pData);
  }

  virtual ULONG GetSMSWake(
      ULONG *                    pbEnabled,
      ULONG *                    pWakeMask) {
    return ::GetSMSWake(
        pbEnabled,
        pWakeMask);
  }

  virtual ULONG SetSMSWake(
      ULONG                      bEnable,
      ULONG                      wakeMask) {
    return ::SetSMSWake(
        bEnable,
        wakeMask);
  }

  virtual ULONG OMADMStartSession(ULONG sessionType) {
    return ::OMADMStartSession(sessionType);
  }

  virtual ULONG OMADMCancelSession() {
    return ::OMADMCancelSession();
  }

  virtual ULONG OMADMGetSessionInfo(
      ULONG *                    pSessionState,
      ULONG *                    pSessionType,
      ULONG *                    pFailureReason,
      BYTE *                     pRetryCount,
      WORD *                     pSessionPause,
      WORD *                     pTimeRemaining) {
    return ::OMADMGetSessionInfo(
        pSessionState,
        pSessionType,
        pFailureReason,
        pRetryCount,
        pSessionPause,
        pTimeRemaining);
  }

  virtual ULONG OMADMGetPendingNIA(
      ULONG *                    pSessionType,
      USHORT *                   pSessionID) {
    return ::OMADMGetPendingNIA(
        pSessionType,
        pSessionID);
  }

  virtual ULONG OMADMSendSelection(
      ULONG                      selection,
      USHORT                     sessionID) {
    return ::OMADMSendSelection(
        selection,
        sessionID);
  }

  virtual ULONG OMADMGetFeatureSettings(
      ULONG *                    pbProvisioning,
      ULONG *                    pbPRLUpdate) {
    return ::OMADMGetFeatureSettings(
        pbProvisioning,
        pbPRLUpdate);
  }

  virtual ULONG OMADMSetProvisioningFeature(
      ULONG                      bProvisioning) {
    return ::OMADMSetProvisioningFeature(
        bProvisioning);
  }

  virtual ULONG OMADMSetPRLUpdateFeature(
      ULONG                      bPRLUpdate) {
    return ::OMADMSetPRLUpdateFeature(
        bPRLUpdate);
  }

  virtual ULONG UpgradeFirmware(CHAR * pDestinationPath) {
    return ::UpgradeFirmware(pDestinationPath);
  }

  virtual ULONG GetImageInfo(
      CHAR *                     pPath,
      ULONG *                    pFirmwareID,
      ULONG *                    pTechnology,
      ULONG *                    pCarrier,
      ULONG *                    pRegion,
      ULONG *                    pGPSCapability) {
    return ::GetImageInfo(
        pPath,
        pFirmwareID,
        pTechnology,
        pCarrier,
        pRegion,
        pGPSCapability);
  }

  virtual ULONG GetImageStore(
      WORD                       pathSize,
      CHAR *                     pImageStorePath) {
    return ::GetImageStore(
        pathSize,
        pImageStorePath);
  }

  virtual ULONG SetSessionStateCallback(tFNSessionState pCallback) {
    return ::SetSessionStateCallback(pCallback);
  }

  virtual ULONG SetByteTotalsCallback(
      tFNByteTotals              pCallback,
      BYTE                       interval) {
    return ::SetByteTotalsCallback(
        pCallback,
        interval);
  }

  virtual ULONG SetDataCapabilitiesCallback(
      tFNDataCapabilities        pCallback) {
    return ::SetDataCapabilitiesCallback(
        pCallback);
  }

  virtual ULONG SetDataBearerCallback(tFNDataBearer pCallback) {
    return ::SetDataBearerCallback(pCallback);
  }

  virtual ULONG SetDormancyStatusCallback(
      tFNDormancyStatus          pCallback) {
    return ::SetDormancyStatusCallback(
        pCallback);
  }

  virtual ULONG SetMobileIPStatusCallback(
      tFNMobileIPStatus          pCallback) {
    return ::SetMobileIPStatusCallback(
        pCallback);
  }

  virtual ULONG SetActivationStatusCallback(
      tFNActivationStatus        pCallback) {
    return ::SetActivationStatusCallback(
        pCallback);
  }

  virtual ULONG SetPowerCallback(tFNPower pCallback) {
    return ::SetPowerCallback(pCallback);
  }
  virtual ULONG SetRoamingIndicatorCallback(
      tFNRoamingIndicator        pCallback) {
    return ::SetRoamingIndicatorCallback(
        pCallback);
  }

  virtual ULONG SetSignalStrengthCallback(
      tFNSignalStrength          pCallback,
      BYTE                       thresholdsSize,
      INT8 *                     pThresholds) {
    return ::SetSignalStrengthCallback(
        pCallback,
        thresholdsSize,
        pThresholds);
  }

  virtual ULONG SetRFInfoCallback(tFNRFInfo pCallback) {
    return ::SetRFInfoCallback(pCallback);
  }

  virtual ULONG SetLURejectCallback(tFNLUReject pCallback) {
    return ::SetLURejectCallback(pCallback);
  }

  virtual ULONG SetNewSMSCallback(tFNNewSMS pCallback) {
    return ::SetNewSMSCallback(pCallback);
  }

  virtual ULONG SetNMEACallback(tFNNewNMEA pCallback) {
    return ::SetNMEACallback(pCallback);
  }

  virtual ULONG SetNMEAPlusCallback(tFNNewNMEAPlus pCallback) {
    return ::SetNMEAPlusCallback(pCallback);
  }

  virtual ULONG SetPDSStateCallback(tFNPDSState pCallback) {
    return ::SetPDSStateCallback(pCallback);
  }

  virtual ULONG SetCATEventCallback(
      tFNCATEvent                pCallback,
      ULONG                      eventMask,
      ULONG *                    pErrorMask) {
    return ::SetCATEventCallback(
        pCallback,
        eventMask,
        pErrorMask);
  }

  virtual ULONG SetOMADMAlertCallback(tFNOMADMAlert pCallback) {
    return ::SetOMADMAlertCallback(pCallback);
  }

  virtual ULONG SetOMADMStateCallback(tFNOMADMState pCallback) {
    return ::SetOMADMStateCallback(pCallback);
  }
};  // Class Sdk
}   // Namespace Gobi

#endif /* PLUGIN_GOBI_SDK_WRAPPER_H_ */

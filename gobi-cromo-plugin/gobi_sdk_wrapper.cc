/*===========================================================================
  FILE:
  gobi_sdk_wrapper.cc,
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

#include "gobi_sdk_wrapper.h"

#include <stdlib.h>  // For NULL

#include "QCWWANCMAPI2k.h"


namespace gobi {
ULONG Sdk::QCWWANEnumerateDevices(
    BYTE *                     pDevicesSize,
    BYTE *                     pDevices) {
  return ::QCWWANEnumerateDevices(pDevicesSize,
                                  pDevices);
}

ULONG Sdk::QCWWANConnect(
    CHAR *                     pDeviceNode,
    CHAR *                     pDeviceKey) {
  return ::QCWWANConnect(pDeviceNode,
                         pDeviceKey);
}

ULONG Sdk::QCWWANDisconnect() {
  return ::QCWWANDisconnect();
}

ULONG Sdk::QCWWANGetConnectedDeviceID(
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

ULONG Sdk::GetSessionState(ULONG * pState) {
  return ::GetSessionState(pState);
}

ULONG Sdk::GetSessionDuration(ULONGLONG * pDuration) {
  return ::GetSessionDuration(pDuration);
}

ULONG Sdk::GetDormancyState(ULONG * pState) {
  return ::GetDormancyState(pState);
}

ULONG Sdk::GetAutoconnect(ULONG * pSetting) {
  return ::GetAutoconnect(pSetting);
}

ULONG Sdk::SetAutoconnect(ULONG setting) {
  return ::SetAutoconnect(setting);
}

ULONG Sdk::SetDefaultProfile(
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

ULONG Sdk::GetDefaultProfile(
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

ULONG Sdk::StartDataSession(
    ULONG *                    pTechnology,
    CHAR *                     pAPNName,
    ULONG *                    pAuthentication,
    CHAR *                     pUsername,
    CHAR *                     pPassword,
    ULONG *                    pSessionId,
    ULONG *                    pFailureReason
                            ) {
  return ::StartDataSession(
      pTechnology,
      NULL,
      NULL,
      NULL,
      NULL,
      pAPNName,
      NULL,
      pAuthentication,
      pUsername,
      pPassword,
      pSessionId,
      pFailureReason);
}

ULONG Sdk::StopDataSession(ULONG sessionId) {
  return ::StopDataSession(sessionId);
}

ULONG Sdk::GetIPAddress(ULONG * pIPAddress) {
  return ::GetIPAddress(pIPAddress);
}

ULONG Sdk::GetConnectionRate(
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

ULONG Sdk::GetPacketStatus(
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

ULONG Sdk::GetByteTotals(
    ULONGLONG *                pTXTotalBytes,
    ULONGLONG *                pRXTotalBytes) {
  return ::GetByteTotals(
      pTXTotalBytes,
      pRXTotalBytes);
}

ULONG Sdk::SetMobileIP(ULONG mode) {
  return ::SetMobileIP(mode);
}

ULONG Sdk::GetMobileIP(ULONG * pMode) {
  return ::GetMobileIP(pMode);
}

ULONG Sdk::SetActiveMobileIPProfile(
    CHAR *                     pSPC,
    BYTE                       index) {
  return ::SetActiveMobileIPProfile(
      pSPC,
      index);
}

ULONG Sdk::GetActiveMobileIPProfile(BYTE * pIndex) {
  return ::GetActiveMobileIPProfile(pIndex);
}

ULONG Sdk::SetMobileIPProfile(
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

ULONG Sdk::GetMobileIPProfile(
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

ULONG Sdk::SetMobileIPParameters(
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

ULONG Sdk::GetMobileIPParameters(
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

ULONG Sdk::GetLastMobileIPError(ULONG * pError) {
  return ::GetLastMobileIPError(pError);
}

ULONG Sdk::GetANAAAAuthenticationStatus(ULONG * pStatus) {
  return ::GetANAAAAuthenticationStatus(pStatus);
}

ULONG Sdk::GetSignalStrengths(
    ULONG *                    pArraySizes,
    INT8 *                     pSignalStrengths,
    ULONG *                    pRadioInterfaces) {
  return ::GetSignalStrengths(
      pArraySizes,
      pSignalStrengths,
      pRadioInterfaces);
}

ULONG Sdk::GetRFInfo(
    BYTE *                     pInstanceSize,
    BYTE *                     pInstances) {
  return ::GetRFInfo(
      pInstanceSize,
      pInstances);
}

ULONG Sdk::PerformNetworkScan(
    BYTE *                     pInstanceSize,
    BYTE *                     pInstances) {
  return ::PerformNetworkScan(
      pInstanceSize,
      pInstances);
}

ULONG Sdk::InitiateNetworkRegistration(
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

ULONG Sdk::InitiateDomainAttach(ULONG action) {
  return ::InitiateDomainAttach(action);
}

ULONG Sdk::GetServingNetwork(
    ULONG *                    pRegistrationState,
    BYTE *                     pRadioIfacesSize,
    BYTE *                     pRadioIfaces,
    ULONG *                    pRoaming) {
  CHAR netName[32];
  ULONG l1, l2, l3;
  WORD w4, w5;
  return ::GetServingNetwork(
      pRegistrationState,
      &l1,              // CS domain
      &l2,              // PS domain
      &l3,              // radio access network type
      pRadioIfacesSize,
      pRadioIfaces,
      pRoaming,
      &w4,              // mobile country code
      &w5,              // mobile network code
      sizeof(netName),  // size of name array
      netName           // network name
                             );
}

ULONG Sdk::GetServingNetworkCapabilities(
    BYTE *                     pDataCapsSize,
    BYTE *                     pDataCaps) {
  return ::GetServingNetworkCapabilities(
      pDataCapsSize,
      pDataCaps);
}

ULONG Sdk::GetDataBearerTechnology(ULONG * pDataBearer) {
  return ::GetDataBearerTechnology(pDataBearer);
}

ULONG Sdk::GetHomeNetwork(
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

ULONG Sdk::SetNetworkPreference(
    ULONG                      technologyPref,
    ULONG                      duration) {
  return ::SetNetworkPreference(
      technologyPref,
      duration);
}

ULONG Sdk::GetNetworkPreference(
    ULONG *                    pTechnologyPref,
    ULONG *                    pDuration,
    ULONG *                    pPersistentTechnologyPref) {
  return ::GetNetworkPreference(
      pTechnologyPref,
      pDuration,
      pPersistentTechnologyPref);
}

ULONG Sdk::SetCDMANetworkParameters(
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

ULONG Sdk::GetCDMANetworkParameters(
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

ULONG Sdk::GetACCOLC(BYTE * pACCOLC) {
  return ::GetACCOLC(pACCOLC);
}

ULONG Sdk::SetACCOLC(
    CHAR *                     pSPC,
    BYTE                       accolc) {
  return ::SetACCOLC(
      pSPC,
      accolc);
}

ULONG Sdk::GetDeviceCapabilities(
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

ULONG Sdk::GetManufacturer(
    BYTE                       stringSize,
    CHAR *                     pString) {
  return ::GetManufacturer(
      stringSize,
      pString);
}

ULONG Sdk::GetModelID(
    BYTE                       stringSize,
    CHAR *                     pString) {
  return ::GetModelID(
      stringSize,
      pString);
}

ULONG Sdk::GetFirmwareRevision(
    BYTE                       stringSize,
    CHAR *                     pString) {
  return ::GetFirmwareRevision(
      stringSize,
      pString);
}

ULONG Sdk::GetFirmwareRevisions(
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

ULONG Sdk::GetFirmwareInfo(
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

ULONG Sdk::GetVoiceNumber(
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

ULONG Sdk::GetIMSI(
    BYTE                       stringSize,
    CHAR *                     pString) {
  return ::GetIMSI(
      stringSize,
      pString);
}

ULONG Sdk::GetSerialNumbers(
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

ULONG Sdk::SetLock(
    ULONG                      state,
    CHAR *                     pCurrentPIN) {
  return ::SetLock(
      state,
      pCurrentPIN);
}

ULONG Sdk::QueryLock(ULONG * pState) {
  return ::QueryLock(pState);
}

ULONG Sdk::ChangeLockPIN(
    CHAR *                     pCurrentPIN,
    CHAR *                     pDesiredPIN) {
  return ::ChangeLockPIN(
      pCurrentPIN,
      pDesiredPIN);
}

ULONG Sdk::GetHardwareRevision(
    BYTE                       stringSize,
    CHAR *                     pString) {
  return ::GetHardwareRevision(
      stringSize,
      pString);
}

ULONG Sdk::GetPRLVersion(WORD * pPRLVersion) {
  return ::GetPRLVersion(pPRLVersion);
}

ULONG Sdk::GetERIFile(
    ULONG *                    pFileSize,
    BYTE *                     pFile) {
  return ::GetERIFile(
      pFileSize,
      pFile);
}

// NB: CHROMIUM: modified this to const after investigating QC code
ULONG Sdk::ActivateAutomatic(const CHAR * pActivationCode) {
  return ::ActivateAutomatic(const_cast<CHAR *>(pActivationCode));
}

ULONG Sdk::ActivateManual(
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

ULONG Sdk::ResetToFactoryDefaults(CHAR * pSPC) {
  return ::ResetToFactoryDefaults(pSPC);
}

ULONG Sdk::GetActivationState(ULONG * pActivationState) {
  return ::GetActivationState(pActivationState);
}

ULONG Sdk::SetPower(ULONG powerMode) {
  return ::SetPower(powerMode);
}

ULONG Sdk::GetPower(ULONG * pPowerMode) {
  return ::GetPower(pPowerMode);
}

ULONG Sdk::GetOfflineReason(
    ULONG *                    pReasonMask,
    ULONG *                    pbPlatform) {
  return ::GetOfflineReason(
      pReasonMask,
      pbPlatform);
}

ULONG Sdk::GetNetworkTime(
    ULONGLONG *                pTimeCount,
    ULONG *                    pTimeSource) {
  return ::GetNetworkTime(
      pTimeCount,
      pTimeSource);
}

ULONG Sdk::ValidateSPC(CHAR * pSPC) {
  return ::ValidateSPC(pSPC);
}

ULONG Sdk::DeleteSMS(
    ULONG                      storageType,
    ULONG *                    pMessageIndex,
    ULONG *                    pMessageTag) {
  return ::DeleteSMS(
      storageType,
      pMessageIndex,
      pMessageTag);
}

ULONG Sdk::GetSMSList(
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

ULONG Sdk::GetSMS(
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

ULONG Sdk::ModifySMSStatus(
    ULONG                      storageType,
    ULONG                      messageIndex,
    ULONG                      messageTag) {
  return ::ModifySMSStatus(
      storageType,
      messageIndex,
      messageTag);
}

ULONG Sdk::SaveSMS(
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

ULONG Sdk::SendSMS(
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

ULONG Sdk::GetSMSCAddress(
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

ULONG Sdk::SetSMSCAddress(
    CHAR *                     pSMSCAddress,
    CHAR *                     pSMSCType) {
  return ::SetSMSCAddress(
      pSMSCAddress,
      pSMSCType);
}

ULONG Sdk::UIMSetPINProtection(
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

ULONG Sdk::UIMVerifyPIN(
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

ULONG Sdk::UIMUnblockPIN(
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

ULONG Sdk::UIMChangePIN(
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

ULONG Sdk::UIMGetPINStatus(
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

ULONG Sdk::UIMGetICCID(
    BYTE                       stringSize,
    CHAR *                     pString) {
  return ::UIMGetICCID(
      stringSize,
      pString);
}
ULONG Sdk::UIMGetControlKeyStatus(
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

ULONG Sdk::UIMSetControlKeyProtection(
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

ULONG Sdk::UIMUnblockControlKey(
    ULONG                      id,
    CHAR *                     pValue,
    ULONG *                    pUnblockRetriesLeft) {
  return ::UIMUnblockControlKey(
      id,
      pValue,
      pUnblockRetriesLeft);
}

ULONG Sdk::GetPDSState(
    ULONG *                    pEnabled,
    ULONG *                    pTracking) {
  return ::GetPDSState(
      pEnabled,
      pTracking);
}

ULONG Sdk::SetPDSState(ULONG enable) {
  return ::SetPDSState(enable);
}

ULONG Sdk::PDSInjectTimeReference(
    ULONGLONG                  systemTime,
    USHORT                     systemDiscontinuities) {
  return ::PDSInjectTimeReference(
      systemTime,
      systemDiscontinuities);
}

ULONG Sdk::GetPDSDefaults(
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

ULONG Sdk::SetPDSDefaults(
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

ULONG Sdk::GetXTRAAutomaticDownload(
    ULONG *                    pbEnabled,
    USHORT *                   pInterval) {
  return ::GetXTRAAutomaticDownload(
      pbEnabled,
      pInterval);
}

ULONG Sdk::SetXTRAAutomaticDownload(
    ULONG                      bEnabled,
    USHORT                     interval) {
  return ::SetXTRAAutomaticDownload(
      bEnabled,
      interval);
}

ULONG Sdk::GetXTRANetwork(ULONG * pPreference) {
  return ::GetXTRANetwork(pPreference);
}

ULONG Sdk::SetXTRANetwork(ULONG preference) {
  return ::SetXTRANetwork(preference);
}

ULONG Sdk::GetXTRAValidity(
    USHORT *                   pGPSWeek,
    USHORT *                   pGPSWeekOffset,
    USHORT *                   pDuration) {
  return ::GetXTRAValidity(
      pGPSWeek,
      pGPSWeekOffset,
      pDuration);
}

ULONG Sdk::ForceXTRADownload() {
  return ::ForceXTRADownload();
}

ULONG Sdk::GetAGPSConfig(
    ULONG *                    pServerAddress,
    ULONG *                    pServerPort) {
  return ::GetAGPSConfig(
      pServerAddress,
      pServerPort);
}

ULONG Sdk::SetAGPSConfig(
    ULONG                      serverAddress,
    ULONG                      serverPort) {
  return ::SetAGPSConfig(
      serverAddress,
      serverPort);
}

ULONG Sdk::GetServiceAutomaticTracking(ULONG * pbAuto) {
  return ::GetServiceAutomaticTracking(pbAuto);
}

ULONG Sdk::SetServiceAutomaticTracking(ULONG bAuto) {
  return ::SetServiceAutomaticTracking(bAuto);
}

ULONG Sdk::GetPortAutomaticTracking(ULONG * pbAuto) {
  return ::GetPortAutomaticTracking(pbAuto);
}

ULONG Sdk::SetPortAutomaticTracking(ULONG bAuto) {
  return ::SetPortAutomaticTracking(bAuto);
}

ULONG Sdk::ResetPDSData(
    ULONG *                    pGPSDataMask,
    ULONG *                    pCellDataMask) {
  return ::ResetPDSData(
      pGPSDataMask,
      pCellDataMask);
}

ULONG Sdk::CATSendTerminalResponse(
    ULONG                      refID,
    ULONG                      dataLen,
    BYTE *                     pData) {
  return ::CATSendTerminalResponse(
      refID,
      dataLen,
      pData);
}

ULONG Sdk::CATSendEnvelopeCommand(
    ULONG                      cmdID,
    ULONG                      dataLen,
    BYTE *                     pData) {
  return ::CATSendEnvelopeCommand(
      cmdID,
      dataLen,
      pData);
}

ULONG Sdk::GetSMSWake(
    ULONG *                    pbEnabled,
    ULONG *                    pWakeMask) {
  return ::GetSMSWake(
      pbEnabled,
      pWakeMask);
}

ULONG Sdk::SetSMSWake(
    ULONG                      bEnable,
    ULONG                      wakeMask) {
  return ::SetSMSWake(
      bEnable,
      wakeMask);
}

ULONG Sdk::OMADMStartSession(ULONG sessionType) {
  return ::OMADMStartSession(sessionType);
}

ULONG Sdk::OMADMCancelSession() {
  return ::OMADMCancelSession();
}

ULONG Sdk::OMADMGetSessionInfo(
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

ULONG Sdk::OMADMGetPendingNIA(
    ULONG *                    pSessionType,
    USHORT *                   pSessionID) {
  return ::OMADMGetPendingNIA(
      pSessionType,
      pSessionID);
}

ULONG Sdk::OMADMSendSelection(
    ULONG                      selection,
    USHORT                     sessionID) {
  return ::OMADMSendSelection(
      selection,
      sessionID);
}

ULONG Sdk::OMADMGetFeatureSettings(
    ULONG *                    pbProvisioning,
    ULONG *                    pbPRLUpdate) {
  return ::OMADMGetFeatureSettings(
      pbProvisioning,
      pbPRLUpdate);
}

ULONG Sdk::OMADMSetProvisioningFeature(
    ULONG                      bProvisioning) {
  return ::OMADMSetProvisioningFeature(
      bProvisioning);
}

ULONG Sdk::OMADMSetPRLUpdateFeature(
    ULONG                      bPRLUpdate) {
  return ::OMADMSetPRLUpdateFeature(
      bPRLUpdate);
}

ULONG Sdk::UpgradeFirmware(CHAR * pDestinationPath) {
  return ::UpgradeFirmware(pDestinationPath);
}

ULONG Sdk::GetImageInfo(
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

ULONG Sdk::GetImageStore(
    WORD                       pathSize,
    CHAR *                     pImageStorePath) {
  return ::GetImageStore(
      pathSize,
      pImageStorePath);
}

ULONG Sdk::SetSessionStateCallback(tFNSessionState pCallback) {
  return ::SetSessionStateCallback(pCallback);
}

ULONG Sdk::SetByteTotalsCallback(
    tFNByteTotals              pCallback,
    BYTE                       interval) {
  return ::SetByteTotalsCallback(
      pCallback,
      interval);
}

ULONG Sdk::SetDataCapabilitiesCallback(
    tFNDataCapabilities        pCallback) {
  return ::SetDataCapabilitiesCallback(
      pCallback);
}

ULONG Sdk::SetDataBearerCallback(tFNDataBearer pCallback) {
  return ::SetDataBearerCallback(pCallback);
}

ULONG Sdk::SetDormancyStatusCallback(
    tFNDormancyStatus          pCallback) {
  return ::SetDormancyStatusCallback(
      pCallback);
}

ULONG Sdk::SetMobileIPStatusCallback(
    tFNMobileIPStatus          pCallback) {
  return ::SetMobileIPStatusCallback(
      pCallback);
}

ULONG Sdk::SetActivationStatusCallback(
    tFNActivationStatus        pCallback) {
  return ::SetActivationStatusCallback(
      pCallback);
}

ULONG Sdk::SetPowerCallback(tFNPower pCallback) {
  return ::SetPowerCallback(pCallback);
}
ULONG Sdk::SetRoamingIndicatorCallback(
    tFNRoamingIndicator        pCallback) {
  return ::SetRoamingIndicatorCallback(
      pCallback);
}

ULONG Sdk::SetSignalStrengthCallback(
    tFNSignalStrength          pCallback,
    BYTE                       thresholdsSize,
    INT8 *                     pThresholds) {
  return ::SetSignalStrengthCallback(
      pCallback,
      thresholdsSize,
      pThresholds);
}

ULONG Sdk::SetRFInfoCallback(tFNRFInfo pCallback) {
  return ::SetRFInfoCallback(pCallback);
}

ULONG Sdk::SetLURejectCallback(tFNLUReject pCallback) {
  return ::SetLURejectCallback(pCallback);
}

ULONG Sdk::SetNewSMSCallback(tFNNewSMS pCallback) {
  return ::SetNewSMSCallback(pCallback);
}

ULONG Sdk::SetNMEACallback(tFNNewNMEA pCallback) {
  return ::SetNMEACallback(pCallback);
}

ULONG Sdk::SetNMEAPlusCallback(tFNNewNMEAPlus pCallback) {
  return ::SetNMEAPlusCallback(pCallback);
}

ULONG Sdk::SetPDSStateCallback(tFNPDSState pCallback) {
  return ::SetPDSStateCallback(pCallback);
}

ULONG Sdk::SetCATEventCallback(
    tFNCATEvent                pCallback,
    ULONG                      eventMask,
    ULONG *                    pErrorMask) {
  return ::SetCATEventCallback(
      pCallback,
      eventMask,
      pErrorMask);
}

ULONG Sdk::SetOMADMAlertCallback(tFNOMADMAlert pCallback) {
  return ::SetOMADMAlertCallback(pCallback);
}

ULONG Sdk::SetOMADMStateCallback(tFNOMADMState pCallback) {
  return ::SetOMADMStateCallback(pCallback);
}
}   // Namespace Gobi
